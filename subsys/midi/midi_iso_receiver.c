/**
 * @file
 * @brief MIDI iso receiver driver
 *
 * Driver for MIDI iso receiver
 */
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include "midi_iso_internal.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/iso.h>

#include <midi/midi.h>
#include "midi/midi_types.h"
#include <midi/midi_parser.h>

#include <zephyr/sys/util.h>

#include <zephyr/device.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_iso_receiver
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define NAME_LEN            30

#define BT_LE_SCAN_CUSTOM BT_LE_SCAN_PARAM(BT_LE_SCAN_TYPE_ACTIVE, \
					   BT_LE_SCAN_OPT_NONE, \
					   BT_GAP_SCAN_FAST_INTERVAL, \
					   BT_GAP_SCAN_FAST_WINDOW)


#define PA_RETRY_COUNT 6

static bool         per_adv_found;
static bool         per_adv_lost;
static bool         big_synced;
static bt_addr_le_t per_addr;
static uint8_t      per_sid;
static uint32_t     per_interval_us;

static uint32_t     iso_recv_count;

struct bt_le_per_adv_sync *sync;
struct bt_iso_big *big;
static struct bt_iso_big_sync_param big_sync_param;

struct midi_iso_receiver_dev_data *iso_dev_data;

static K_SEM_DEFINE(sem_per_adv, 0, 1);
static K_SEM_DEFINE(sem_per_sync, 0, 1);
static K_SEM_DEFINE(sem_per_sync_lost, 0, 1);
static K_SEM_DEFINE(sem_per_big_info, 0, 1);


DEFINE_MIDI_PARSER_SERIAL(iso_serial_parser);

// static K_SEM_DEFINE(sem_big_sync, 0, BIS_ISO_CHAN_COUNT);
// static K_SEM_DEFINE(sem_big_sync_lost, 0, BIS_ISO_CHAN_COUNT);

uint16_t previous_seq_num;
uint64_t big_interval;

struct midi_iso_receiver_dev_data {

	struct midi_api *api;

	const struct device *dev;

	void *user_data;

};

int midi_iso_scan(const struct device *dev) 
{
    return bt_le_scan_start(BT_LE_SCAN_CUSTOM, NULL);
}

int midi_iso_receiver_port_callback_set(const struct device *dev,
				 midi_transfer cb,
				 void *user_data)
{
	struct midi_iso_receiver_dev_data *iso_dev_data = dev->data;

	if(iso_dev_data) {
		iso_dev_data->api->midi_transfer_done = cb;
		iso_dev_data->user_data = user_data;
		return 0;
	}

	return -ENOTSUP;
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
    struct bt_le_per_adv_sync_param sync_create_param;
    int err;

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
		
        err = bt_le_scan_stop();
        if (err) {
            LOG_ERR("stop failed (err %d)\n", err);
            return;
        }

        per_adv_found = true;

		per_sid = info->sid;
		per_interval_us = BT_CONN_INTERVAL_TO_US(info->interval);
		bt_addr_le_copy(&per_addr, info->addr);

		bt_addr_le_copy(&sync_create_param.addr, &per_addr);
        sync_create_param.options = 0;
        sync_create_param.sid = per_sid;
        sync_create_param.skip = 0;
        /* Multiple PA interval with retry count and convert to unit of 10 ms */
        sync_create_param.timeout = (per_interval_us * PA_RETRY_COUNT) /
                        (10 * USEC_PER_MSEC);
        err = bt_le_per_adv_sync_create(&sync_create_param, &sync);
        if (err) {
            LOG_ERR("adv sync create failed (err %d)\n", err);
            return;
        }
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

	// k_sem_give(&sem_per_sync);
    
}

static void term_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_term_info *info)
{
	char le_addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	LOG_INF("PER_ADV_SYNC[%u]: [DEVICE]: %s sync terminated\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr);

	per_adv_lost = true;
	// k_sem_give(&sem_per_sync_lost);
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
    int err;

	bt_addr_le_to_str(biginfo->addr, le_addr, sizeof(le_addr));
    big_interval = (biginfo->iso_interval * 5 / 4);
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


	// k_sem_give(&sem_per_big_info);
if (!big_synced) {
    big_synced = true;
    err = bt_iso_big_sync(sync, &big_sync_param, &big);
    if (err) {
        LOG_ERR("big sync failed (err %d)\n", err);
        return;
    }
}
    
}

static struct bt_le_per_adv_sync_cb sync_callbacks = {
	.synced = sync_cb,
	.term = term_cb,
	.recv = recv_cb,
	.biginfo = biginfo_cb,
};

static midi_msg_t * parse_next(struct net_buf *buf) {
    
    midi_msg_t * serial_msg; 
    midi_msg_t * parsed_msg = NULL; 
    uint8_t byte;

    while (!parsed_msg) {
        serial_msg = midi_msg_alloc(NULL, 1);
        byte = net_buf_pull_u8(buf);
        memcpy(serial_msg->data, &byte, 1);
        serial_msg->format = MIDI_FORMAT_1_0_SERIAL;
        serial_msg->len = 1;
        parsed_msg = midi_parse_serial(serial_msg, &iso_serial_parser);
        midi_msg_unref(serial_msg);
        if (parsed_msg) {
            return parsed_msg;
        }
    }
     return NULL;
}

static inline uint16_t calculate_timestamp(int64_t uptime, uint8_t timestamp)
{
	int64_t delta;
	delta = ((127 & timestamp) * (big_interval)) / 128;
	LOG_INF("delta %ld", delta);
    return TIMESTAMP(uptime + delta);
}

static void iso_recv(struct bt_iso_chan *chan, const struct bt_iso_recv_info *info,
		struct net_buf *buf)
{
    int64_t uptime;

	uptime = k_ticks_to_ms_floor64(k_uptime_ticks());
	LOG_INF("Uptime %ld",  TIMESTAMP(uptime) );
	LOG_HEXDUMP_INF(buf->data, buf->len, "data");
    uint8_t new_data_len;
    uint8_t timestamp;
    
    midi_msg_t * parsed_msg; 

    new_data_len = net_buf_remove_u8(buf);

    if ((previous_seq_num + 1) != info->seq_num) {
        LOG_INF("Previous packet was lost process retransmit data!");
        while (buf->len < new_data_len)
        {
            timestamp = net_buf_pull_u8(buf);
            parsed_msg = parse_next(buf);
            if (parsed_msg) {
                if(iso_dev_data->api->midi_transfer_done)  {
                    iso_dev_data->api->midi_transfer_done(
                            iso_dev_data->dev, parsed_msg, iso_dev_data->user_data);
                } else {
                    midi_msg_unref(parsed_msg);
                }
            }
        }
    } else {
        net_buf_pull_mem(buf, buf->len - new_data_len);
        while (buf->len)
        {
            timestamp = net_buf_pull_u8(buf);
            parsed_msg = parse_next(buf);
            if (parsed_msg) {
                parsed_msg->timestamp = calculate_timestamp(uptime, timestamp);
				LOG_INF(" olde %d new timestamp %d", (127 & timestamp), parsed_msg->timestamp);
                if(iso_dev_data->api->midi_transfer_done)  {
                    iso_dev_data->api->midi_transfer_done(
                            iso_dev_data->dev, parsed_msg, iso_dev_data->user_data);
                } else {
                    midi_msg_unref(parsed_msg);
                }
            }
        }
    }
	// for (size_t i = 0; i < buf->len; i++)
	// {	
	// 	serial_msg = midi_msg_alloc(NULL, 1);
	// 	memcpy(serial_msg->data, (void*)(buf->data + i), 1);
	// 	serial_msg->format = MIDI_FORMAT_1_0_SERIAL;
	// 	serial_msg->len = 1;
		
	// 	// serial_msg = midi_msg_alloc(serial_msg, 1);
	// 	// memcpy(serail_msg->data, (void*)(buf->data + i), 1);
	// 	LOG_INF("serial byte: %d", *serial_msg->data); 	
	// 	parsed_msg = midi_parse_serial(serial_msg, &serial_parser);
	// 	midi_msg_unref(serial_msg);
	// 	if (parsed_msg)
	// 	{
	// 		midi_send(serial_midi_out_dev, parsed_msg);
	// 		parsed_msg = NULL;
	// 	}
	// }
	previous_seq_num = info->seq_num;
	iso_recv_count++;
}

static void iso_connected(struct bt_iso_chan *chan)
{
	LOG_INF("ISO Channel %p connected\n", chan);
	// k_sem_give(&sem_big_sync);
}

static void iso_disconnected(struct bt_iso_chan *chan, uint8_t reason)
{
	LOG_INF("ISO Channel %p disconnected with reason 0x%02x\n",
	       chan, reason);

	if (reason != BT_HCI_ERR_OP_CANCELLED_BY_HOST) {
		// k_sem_give(&sem_big_sync_lost);
	}
}

static struct bt_iso_chan_ops iso_ops = {
	.recv		= iso_recv,
	.connected	= iso_connected,
	.disconnected	= iso_disconnected,
};

static struct bt_iso_chan_io_qos iso_rx_qos[1];

static struct bt_iso_chan_qos bis_iso_qos[] = {
	{ .rx = &iso_rx_qos[0], },
};

static struct bt_iso_chan bis_iso_chan[] = {
	{ .ops = &iso_ops,
	  .qos = &bis_iso_qos[0], },
};

static struct bt_iso_chan *bis[] = {
	&bis_iso_chan[0],
};

static struct bt_iso_big_sync_param big_sync_param = {
	.bis_channels = bis,
	.num_bis = 1,
	.bis_bitfield = (BIT_MASK(1) << 1),
	.mse = 1,
	.sync_timeout = 100, /* in 10 ms units */
};

static int midi_iso_receiver_device_init(const struct device *dev)
{
	int err;
    
	// struct bt_le_per_adv_sync_param sync_create_param;


	iso_dev_data = dev->data;

	iso_dev_data->dev = dev;
	iso_dev_data->api = (struct midi_api*)dev->api;

    /* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	bt_le_scan_cb_register(&scan_callbacks);
	bt_le_per_adv_sync_cb_register(&sync_callbacks);
	
    per_adv_lost = false;
    per_adv_found = false;
    big_synced = false;

    return err;
}

#define DEFINE_MIDI_ISO_RECEIVER_DEV_DATA(dev)				        \
    static struct midi_iso_receiver_dev_data midi_iso_receiver_dev_data_##dev;    \

#define DEFINE_MIDI_RECEIVER_DEVICE(dev)					  			\
	static struct midi_api midi_iso_receiver_api_##dev = {			\
		.midi_callback_set = midi_iso_receiver_port_callback_set,	\
	};															\
	DEVICE_DT_DEFINE(MIDI_ISO_RECEIVER_DEV_N_ID(dev),			  			\
			    &midi_iso_receiver_device_init,			  			\
			    NULL,					  						\
			    &midi_iso_receiver_dev_data_##dev,			  		\
			    NULL, APPLICATION,	  							\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  		\
			    &midi_iso_receiver_api_##dev);



#define MIDI_ISO_RECEIVER_DEVICE(dev, _) \
	COND_CODE_1(UTIL_AND(DT_NODE_HAS_COMPAT(RADIO_DEV_N_ID(dev),\
		COMPAT_RADIO_DEVICE),                                               \
        DT_NODE_HAS_STATUS(RADIO_DEV_N_ID(dev), okay)),(        \
	DEFINE_MIDI_ISO_RECEIVER_DEV_DATA(dev)                               \
	COND_NODE_HAS_COMPAT_CHILD(MIDI_ISO_DEV_N_ID(dev),             \
		COMPAT_MIDI_ISO_RECEIVER_DEVICE,                                 \
		(DEFINE_MIDI_RECEIVER_DEVICE(dev)), ())), ())

LISTIFY(MIDI_ISO_DEVICE_COUNT, MIDI_ISO_RECEIVER_DEVICE, ());
