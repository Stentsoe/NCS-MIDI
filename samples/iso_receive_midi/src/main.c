/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/iso.h>
#include <zephyr/sys/byteorder.h>

#include <midi/midi.h>
#include <midi/midi_parser.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME iso_receive_midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define TIMEOUT_SYNC_CREATE K_SECONDS(10)
#define NAME_LEN            30

#define BT_LE_SCAN_CUSTOM BT_LE_SCAN_PARAM(BT_LE_SCAN_TYPE_ACTIVE, \
					   BT_LE_SCAN_OPT_NONE, \
					   BT_GAP_SCAN_FAST_INTERVAL, \
					   BT_GAP_SCAN_FAST_WINDOW)

#define PA_RETRY_COUNT 6

#define BIS_ISO_CHAN_COUNT 2

static bool         per_adv_found;
static bool         per_adv_lost;
static bt_addr_le_t per_addr;
static uint8_t      per_sid;
static uint32_t     per_interval_us;

static uint32_t     iso_recv_count;

static K_SEM_DEFINE(sem_per_adv, 0, 1);
static K_SEM_DEFINE(sem_per_sync, 0, 1);
static K_SEM_DEFINE(sem_per_sync_lost, 0, 1);
static K_SEM_DEFINE(sem_per_big_info, 0, 1);
static K_SEM_DEFINE(sem_big_sync, 0, BIS_ISO_CHAN_COUNT);
static K_SEM_DEFINE(sem_big_sync_lost, 0, BIS_ISO_CHAN_COUNT);

const struct device *serial_midi_out_dev;

DEFINE_MIDI_PARSER_SERIAL(serial_parser);

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led_gpio = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define HAS_LED     1
#define BLINK_ONOFF K_MSEC(500)

static struct k_work_delayable blink_work;
static bool                    led_is_on;
static bool                    blink;

static void blink_timeout(struct k_work *work)
{
	if (!blink) {
		return;
	}

	led_is_on = !led_is_on;
	gpio_pin_set_dt(&led_gpio, (int)led_is_on);

	k_work_schedule(&blink_work, BLINK_ONOFF);
}
#endif


static int midi_serial_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	LOG_INF("MIDI SERIAL sent!");
	
	midi_msg_unref(msg);

	return 0;
}

static bool data_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	uint8_t len;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		len = MIN(data->data_len, NAME_LEN - 1);
		memcpy(name, data->data, len);
		name[len] = '\0';
		return false;
	default:
		return true;
	}
}

static const char *phy2str(uint8_t phy)
{
	switch (phy) {
	case 0: return "No packets";
	case BT_GAP_LE_PHY_1M: return "LE 1M";
	case BT_GAP_LE_PHY_2M: return "LE 2M";
	case BT_GAP_LE_PHY_CODED: return "LE Coded";
	default: return "Unknown";
	}
}

static void scan_recv(const struct bt_le_scan_recv_info *info,
		      struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char name[NAME_LEN];

	(void)memset(name, 0, sizeof(name));

	bt_data_parse(buf, data_cb, name);

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
	LOG_INF("[DEVICE]: %s, AD evt type %u, Tx Pwr: %i, RSSI %i %s "
	       "C:%u S:%u D:%u SR:%u E:%u Prim: %s, Secn: %s, "
	       "Interval: 0x%04x (%u us), SID: %u\n",
	       le_addr, info->adv_type, info->tx_power, info->rssi, name,
	       (info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0,
	       (info->adv_props & BT_GAP_ADV_PROP_SCANNABLE) != 0,
	       (info->adv_props & BT_GAP_ADV_PROP_DIRECTED) != 0,
	       (info->adv_props & BT_GAP_ADV_PROP_SCAN_RESPONSE) != 0,
	       (info->adv_props & BT_GAP_ADV_PROP_EXT_ADV) != 0,
	       phy2str(info->primary_phy), phy2str(info->secondary_phy),
	       info->interval, BT_CONN_INTERVAL_TO_US(info->interval), info->sid);

	if (!per_adv_found && info->interval) {
		per_adv_found = true;

		per_sid = info->sid;
		per_interval_us = BT_CONN_INTERVAL_TO_US(info->interval);
		bt_addr_le_copy(&per_addr, info->addr);

		k_sem_give(&sem_per_adv);
	}
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};

static void sync_cb(struct bt_le_per_adv_sync *sync,
		    struct bt_le_per_adv_sync_synced_info *info)
{
	char le_addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	LOG_INF("PER_ADV_SYNC[%u]: [DEVICE]: %s synced, "
	       "Interval 0x%04x (%u ms), PHY %s\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr,
	       info->interval, info->interval * 5 / 4, phy2str(info->phy));

	k_sem_give(&sem_per_sync);
}

static void term_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_term_info *info)
{
	char le_addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	LOG_INF("PER_ADV_SYNC[%u]: [DEVICE]: %s sync terminated\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr);

	per_adv_lost = true;
	k_sem_give(&sem_per_sync_lost);
}

static void recv_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info,
		    struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char data_str[129];

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
	bin2hex(buf->data, buf->len, data_str, sizeof(data_str));

	LOG_INF("PER_ADV_SYNC[%u]: [DEVICE]: %s, tx_power %i, "
	       "RSSI %i, CTE %u, data length %u, data: %s\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr, info->tx_power,
	       info->rssi, info->cte_type, buf->len, data_str);
}

static void biginfo_cb(struct bt_le_per_adv_sync *sync,
		       const struct bt_iso_biginfo *biginfo)
{
	char le_addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(biginfo->addr, le_addr, sizeof(le_addr));

	LOG_INF("BIG INFO[%u]: [DEVICE]: %s, sid 0x%02x, "
	       "num_bis %u, nse %u, interval 0x%04x (%u ms), "
	       "bn %u, pto %u, irc %u, max_pdu %u, "
	       "sdu_interval %u us, max_sdu %u, phy %s, "
	       "%s framing, %sencrypted\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr, biginfo->sid,
	       biginfo->num_bis, biginfo->sub_evt_count,
	       biginfo->iso_interval,
	       (biginfo->iso_interval * 5 / 4),
	       biginfo->burst_number, biginfo->offset,
	       biginfo->rep_count, biginfo->max_pdu, biginfo->sdu_interval,
	       biginfo->max_sdu, phy2str(biginfo->phy),
	       biginfo->framing ? "with" : "without",
	       biginfo->encryption ? "" : "not ");


	k_sem_give(&sem_per_big_info);
}

static struct bt_le_per_adv_sync_cb sync_callbacks = {
	.synced = sync_cb,
	.term = term_cb,
	.recv = recv_cb,
	.biginfo = biginfo_cb,
};

static void iso_recv(struct bt_iso_chan *chan, const struct bt_iso_recv_info *info,
		struct net_buf *buf)
{
	char data_str[128];
	size_t str_len;
	uint32_t count = 0; /* only valid if the data is a counter */
	midi_msg_t * serial_msg;
	midi_msg_t * parsed_msg;
	// if (buf->len == sizeof(count)) {
	// 	count = sys_get_le32(buf->data);
	// 	if (IS_ENABLED(CONFIG_ISO_ALIGN_PRINT_INTERVALS)) {
	// 		iso_recv_count = count;
	// 	}
	// }

	// if ((iso_recv_count % CONFIG_ISO_PRINT_INTERVAL) == 0) {
	// 	str_len = bin2hex(buf->data, buf->len, data_str, sizeof(data_str));
	// 	LOG_INF("Incoming data channel %p flags 0x%x seq_num %u ts %u len %u: "
	// 	       "%s (counter value %u)\n", chan, info->flags, info->seq_num,
	// 	       info->ts, buf->len, data_str, count);
	// }
	LOG_HEXDUMP_INF(buf->data, buf->len, "data");
	for (size_t i = 0; i < buf->len; i++)
	{	
		serial_msg = midi_msg_alloc(NULL, 1);
		memcpy(serial_msg->data, (void*)(buf->data + i), 1);
		serial_msg->format = MIDI_FORMAT_1_0_SERIAL;
		serial_msg->len = 1;
		
		// serial_msg = midi_msg_alloc(serial_msg, 1);
		// memcpy(serail_msg->data, (void*)(buf->data + i), 1);
		LOG_INF("serial byte: %d", *serial_msg->data); 	
		parsed_msg = midi_parse_serial(serial_msg, &serial_parser);
		midi_msg_unref(serial_msg);
		if (parsed_msg)
		{
			midi_send(serial_midi_out_dev, parsed_msg);
			parsed_msg = NULL;
		}
	}
	
	iso_recv_count++;
}

static void iso_connected(struct bt_iso_chan *chan)
{
	LOG_INF("ISO Channel %p connected\n", chan);
	k_sem_give(&sem_big_sync);
}

static void iso_disconnected(struct bt_iso_chan *chan, uint8_t reason)
{
	LOG_INF("ISO Channel %p disconnected with reason 0x%02x\n",
	       chan, reason);

	if (reason != BT_HCI_ERR_OP_CANCELLED_BY_HOST) {
		k_sem_give(&sem_big_sync_lost);
	}
}

static struct bt_iso_chan_ops iso_ops = {
	.recv		= iso_recv,
	.connected	= iso_connected,
	.disconnected	= iso_disconnected,
};

static struct bt_iso_chan_io_qos iso_rx_qos[BIS_ISO_CHAN_COUNT];

static struct bt_iso_chan_qos bis_iso_qos[] = {
	{ .rx = &iso_rx_qos[0], },
	{ .rx = &iso_rx_qos[1], },
};

static struct bt_iso_chan bis_iso_chan[] = {
	{ .ops = &iso_ops,
	  .qos = &bis_iso_qos[0], },
	{ .ops = &iso_ops,
	  .qos = &bis_iso_qos[1], },
};

static struct bt_iso_chan *bis[] = {
	&bis_iso_chan[0],
	&bis_iso_chan[1],
};

static struct bt_iso_big_sync_param big_sync_param = {
	.bis_channels = bis,
	.num_bis = BIS_ISO_CHAN_COUNT,
	.bis_bitfield = (BIT_MASK(BIS_ISO_CHAN_COUNT) << 1),
	.mse = 1,
	.sync_timeout = 100, /* in 10 ms units */
};


const struct device * get_port(const char* name, 
			midi_transfer cb, void *user_data)
{
	int err;
	const struct device * dev;
	dev = device_get_binding(name);
	if (!dev) {
		LOG_ERR("Can not get device: %s", name);
	}
	err = midi_callback_set(dev, cb, user_data);
	if (err != 0) {
		LOG_ERR("Can not set callbacks for device: %s", name);
	}
	return dev;
}

int main(void)
{
	struct bt_le_per_adv_sync_param sync_create_param;
	struct bt_le_per_adv_sync *sync;
	struct bt_iso_big *big;
	uint32_t sem_timeout_us;
	int err;

	iso_recv_count = 0;

	LOG_INF("Starting Synchronized Receiver Demo\n");

	serial_midi_out_dev = get_port("SERIAL_MIDI_OUT", 
		midi_serial_sent, NULL);

#if defined(HAS_LED)
	LOG_INF("Get reference to LED device...");

	if (!device_is_ready(led_gpio.port)) {
		LOG_ERR("LED gpio device not ready.\n");
		return 0;
	}
	LOG_INF("done.\n");

	LOG_INF("Configure GPIO pin...");
	err = gpio_pin_configure_dt(&led_gpio, GPIO_OUTPUT_ACTIVE);
	if (err) {
		return 0;
	}
	LOG_INF("done.\n");

	k_work_init_delayable(&blink_work, blink_timeout);
#endif /* HAS_LED */

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	LOG_INF("Scan callbacks register...");
	bt_le_scan_cb_register(&scan_callbacks);
	LOG_INF("success.\n");

	LOG_INF("Periodic Advertising callbacks register...");
	bt_le_per_adv_sync_cb_register(&sync_callbacks);
	LOG_INF("Success.\n");

	do {
		per_adv_lost = false;

		LOG_INF("Start scanning...");
		err = bt_le_scan_start(BT_LE_SCAN_CUSTOM, NULL);
		if (err) {
			LOG_ERR("failed (err %d)\n", err);
			return 0;
		}
		LOG_INF("success.\n");

#if defined(HAS_LED)
		LOG_INF("Start blinking LED...\n");
		led_is_on = false;
		blink = true;
		gpio_pin_set_dt(&led_gpio, (int)led_is_on);
		k_work_reschedule(&blink_work, BLINK_ONOFF);
#endif /* HAS_LED */

		LOG_INF("Waiting for periodic advertising...\n");
		per_adv_found = false;
		err = k_sem_take(&sem_per_adv, K_FOREVER);
		if (err) {
			LOG_ERR("failed (err %d)\n", err);
			return 0;
		}
		LOG_INF("Found periodic advertising.\n");

		LOG_INF("Stop scanning...");
		err = bt_le_scan_stop();
		if (err) {
			LOG_ERR("failed (err %d)\n", err);
			return 0;
		}
		LOG_INF("success.\n");

		LOG_INF("Creating Periodic Advertising Sync...");
		bt_addr_le_copy(&sync_create_param.addr, &per_addr);
		sync_create_param.options = 0;
		sync_create_param.sid = per_sid;
		sync_create_param.skip = 0;
		/* Multiple PA interval with retry count and convert to unit of 10 ms */
		sync_create_param.timeout = (per_interval_us * PA_RETRY_COUNT) /
						(10 * USEC_PER_MSEC);
		sem_timeout_us = per_interval_us * PA_RETRY_COUNT;
		err = bt_le_per_adv_sync_create(&sync_create_param, &sync);
		if (err) {
			LOG_ERR("failed (err %d)\n", err);
			return 0;
		}
		LOG_INF("success.\n");

		LOG_INF("Waiting for periodic sync...\n");
		err = k_sem_take(&sem_per_sync, K_USEC(sem_timeout_us));
		if (err) {
			LOG_ERR("failed (err %d)\n", err);

			LOG_INF("Deleting Periodic Advertising Sync...");
			err = bt_le_per_adv_sync_delete(sync);
			if (err) {
				LOG_ERR("failed (err %d)\n", err);
				return 0;
			}
			continue;
		}
		LOG_INF("Periodic sync established.\n");

		LOG_INF("Waiting for BIG info...\n");
		err = k_sem_take(&sem_per_big_info, K_USEC(sem_timeout_us));
		if (err) {
			LOG_ERR("failed (err %d)\n", err);

			if (per_adv_lost) {
				continue;
			}

			LOG_ERR("Deleting Periodic Advertising Sync...");
			err = bt_le_per_adv_sync_delete(sync);
			if (err) {
				LOG_ERR("failed (err %d)\n", err);
				return 0;
			}
			continue;
		}
		LOG_INF("Periodic sync established.\n");

big_sync_create:
		LOG_INF("Create BIG Sync...\n");
		err = bt_iso_big_sync(sync, &big_sync_param, &big);
		if (err) {
			LOG_ERR("failed (err %d)\n", err);
			return 0;
		}
		LOG_INF("success.\n");

		for (uint8_t chan = 0U; chan < BIS_ISO_CHAN_COUNT; chan++) {
			LOG_INF("Waiting for BIG sync chan %u...\n", chan);
			err = k_sem_take(&sem_big_sync, TIMEOUT_SYNC_CREATE);
			if (err) {
				break;
			}
			LOG_INF("BIG sync chan %u successful.\n", chan);
		}
		if (err) {
			LOG_ERR("failed (err %d)\n", err);

			LOG_INF("BIG Sync Terminate...");
			err = bt_iso_big_terminate(big);
			if (err) {
				LOG_ERR("failed (err %d)\n", err);
				return 0;
			}
			LOG_INF("done.\n");

			goto per_sync_lost_check;
		}
		LOG_INF("BIG sync established.\n");

#if defined(HAS_LED)
		LOG_INF("Stop blinking LED.\n");
		blink = false;
		/* If this fails, we'll exit early in the handler because blink
		 * is false.
		 */
		k_work_cancel_delayable(&blink_work);

		/* Keep LED on */
		led_is_on = true;
		gpio_pin_set_dt(&led_gpio, (int)led_is_on);
#endif /* HAS_LED */

		for (uint8_t chan = 0U; chan < BIS_ISO_CHAN_COUNT; chan++) {
			LOG_INF("Waiting for BIG sync lost chan %u...\n", chan);
			err = k_sem_take(&sem_big_sync_lost, K_FOREVER);
			if (err) {
				LOG_ERR("failed (err %d)\n", err);
				return 0;
			}
			LOG_INF("BIG sync lost chan %u.\n", chan);
		}
		LOG_INF("BIG sync lost.\n");

per_sync_lost_check:
		LOG_INF("Check for periodic sync lost...\n");
		err = k_sem_take(&sem_per_sync_lost, K_NO_WAIT);
		if (err) {
			/* Periodic Sync active, go back to creating BIG Sync */
			goto big_sync_create;
		}
		LOG_INF("Periodic sync lost.\n");
	} while (true);
}
