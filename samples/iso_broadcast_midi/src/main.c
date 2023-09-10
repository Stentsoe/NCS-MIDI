/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/iso.h>
#include <zephyr/sys/byteorder.h>

#include <lll_adv_iso.h>

#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <midi/midi.h>
#include <midi/midi_parser.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME iso_broadcast_midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

#define BUF_ALLOC_TIMEOUT (10) /* milliseconds */
#define BIG_SDU_INTERVAL_US (5000)

static K_FIFO_DEFINE(fifo_tx_data);
NET_BUF_POOL_FIXED_DEFINE(bis_tx_pool, 1,
			  BT_ISO_SDU_BUF_SIZE(CONFIG_BT_ISO_TX_MTU),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

static K_SEM_DEFINE(sem_big_cmplt, 0, 1);
static K_SEM_DEFINE(sem_big_term, 0, 1);


static uint16_t seq_num;
static struct k_work_delayable send_work;

uint8_t iso_send_count = 0;
uint32_t dummy = 0;
uint8_t iso_data[sizeof(dummy)] = { 0 };

const struct device *serial_midi_in_dev;

DEFINE_MIDI_PARSER_SERIAL(serial_parser);

static int midi_serial_received(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	midi_msg_t *parsed_msg = midi_parse_serial(msg, &serial_parser);

	midi_msg_unref(msg);
	if (parsed_msg) {
		k_fifo_put(&fifo_tx_data, parsed_msg);
		LOG_HEXDUMP_INF(parsed_msg->data, parsed_msg->len, "parsed:");
		// midi_send(serial_midi_out_dev, parsed_msg);
	}
	
	return 0;
}

static void iso_connected(struct bt_iso_chan *chan)
{
	LOG_INF("ISO Channel %p connected", chan);

	seq_num = 0U;

	k_sem_give(&sem_big_cmplt);
}

static void iso_disconnected(struct bt_iso_chan *chan, uint8_t reason)
{
	LOG_INF("ISO Channel %p disconnected with reason 0x%02x",
	       chan, reason);
	k_sem_give(&sem_big_term);
}

static struct bt_iso_chan_ops iso_ops = {
	.connected	= iso_connected,
	.disconnected	= iso_disconnected,
};

static struct bt_iso_chan_io_qos iso_tx_qos = {
	.sdu = CONFIG_BT_ISO_TX_MTU, /* bytes */
	.rtn = 3,
	.phy = BT_GAP_LE_PHY_2M,
};

static struct bt_iso_chan_qos bis_iso_qos = {
	.tx = &iso_tx_qos,
};

static struct bt_iso_chan bis_iso_chan[] = {
	{ .ops = &iso_ops, .qos = &bis_iso_qos, }
};

static struct bt_iso_chan *bis[] = {
	&bis_iso_chan[0]
};

static struct bt_iso_big_create_param big_create_param = {
	.num_bis = 1,
	.bis_channels = bis,
	.interval = BIG_SDU_INTERVAL_US, /* in microseconds */
	.latency = 10, /* in milliseconds */
	.packing = 0, /* 0 - sequential, 1 - interleaved */
	.framing = 0, /* 0 - unframed, 1 - framed */
};

static void radio_isr_handler(void)
{
	// LOG_INF("radio isr");
	k_work_schedule(&send_work, K_USEC(big_create_param.interval-500));
}

static void send_work_handler(struct k_work *work)
{
	midi_msg_t *msg;
	struct net_buf *buf;
	int ret;

	// LOG_INF("send_work_handler");

	buf = net_buf_alloc(&bis_tx_pool, K_MSEC(BUF_ALLOC_TIMEOUT));
	if (!buf) {
		LOG_ERR("Data buffer allocate timeout on channel");
		return;
	}
	net_buf_reserve(buf, BT_ISO_CHAN_SEND_RESERVE);

	do {
		msg = k_fifo_get(&fifo_tx_data, K_NO_WAIT);
		if(msg) {
			LOG_HEXDUMP_INF(msg->data, msg->len,"pdu");
			net_buf_add_mem(buf, msg->data, msg->len);
			midi_msg_unref(msg);
		}
		
	} while (msg);
	
	// if (buf->len == 0)
	// {
	// 	LOG_INF("No data this interval send dummy");
	// 	net_buf_add_mem(buf, &dummy, sizeof(dummy));
	// }
	ret = bt_iso_chan_send(&bis_iso_chan[0], buf,
					seq_num, BT_ISO_TIMESTAMP_NONE);
	if (ret < 0) {
		LOG_ERR("Unable to broadcast data on"
				" : %d", ret);
		net_buf_unref(buf);
		return;
	}
	
	iso_send_count++;
	seq_num++;
}

int main(void)
{
	int err;
	struct bt_le_ext_adv *adv;
	struct bt_iso_big *big;
	int blink_status; 

	blink_status = 0;

	LOG_INF("Starting ISO Broadcast Demo");
	
	serial_midi_in_dev = device_get_binding("SERIAL_MIDI_IN");
	if (!serial_midi_in_dev) {
		LOG_ERR("Can not get serial MIDI Device");
	}

	err = midi_callback_set(serial_midi_in_dev, midi_serial_received, NULL);
	if (err != 0) {
		LOG_ERR("Can not set MIDI IN callbacks");
	}

	lll_adv_iso_radio_isr_cb_set(radio_isr_handler);
	k_work_init_delayable(&send_work, send_work_handler);

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	/* Create a non-connectable non-scannable advertising set */
	err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN_NAME, NULL, &adv);
	if (err) {
		LOG_ERR("Failed to create advertising set (err %d)", err);
		return 0;
	}

	/* Set periodic advertising parameters */
	err = bt_le_per_adv_set_param(adv, BT_LE_PER_ADV_DEFAULT);
	if (err) {
		LOG_ERR("Failed to set periodic advertising parameters"
		       " (err %d)", err);
		return 0;
	}

	/* Enable Periodic Advertising */
	err = bt_le_per_adv_start(adv);
	if (err) {
		LOG_ERR("Failed to enable periodic advertising (err %d)", err);
		return 0;
	}

	/* Start extended advertising */
	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		LOG_ERR("Failed to start extended advertising (err %d)", err);
		return 0;
	}

	/* Create BIG */
	err = bt_iso_big_create(adv, &big_create_param, &big);
	if (err) {
		LOG_ERR("Failed to create BIG (err %d)", err);
		return 0;
	}

	
	LOG_INF("Waiting for BIG complete");
	err = k_sem_take(&sem_big_cmplt, K_FOREVER);
	if (err) {
		LOG_ERR("failed (err %d)", err);
		return 0;
	}
	LOG_INF("BIG create complete chan.");


	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}
