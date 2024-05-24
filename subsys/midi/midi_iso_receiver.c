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
#include <midi/midi_ump.h>
#include "midi/midi_types.h"
#include <midi/midi_parser.h>
#include <midi/midi_sysex.h>

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

uint8_t previous_msg_num = 0xFF;

struct bt_le_per_adv_sync *sync;
struct bt_iso_big *big;
static struct bt_iso_big_sync_param big_sync_param;

struct midi_iso_receiver_dev_data *iso_dev_data;

DEFINE_MIDI_PARSER_SERIAL(iso_serial_parser);

uint16_t previous_seq_num;
uint64_t big_interval;

struct midi_iso_receiver_dev_data {
	struct midi_api *api;
	const struct device *dev;
	void *user_data;
};

uint32_t muid_subscribe;

int midi_iso_set_function_block(midi_ump_function_block_t *function_block)
{
	muid_subscribe = function_block->muid;
	return 0;
}

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
	LOG_DBG("[DEVICE]: %s, AD evt type %u, Tx Pwr: %i, RSSI %i %s "
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

	LOG_DBG("PER_ADV_SYNC[%u]: [DEVICE]: %s synced, "
	       "Interval 0x%04x (%u ms), PHY %s\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr,
	       info->interval, info->interval * 5 / 4, phy2str(info->phy));
    
}

static void term_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_term_info *info)
{
	char le_addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	LOG_DBG("PER_ADV_SYNC[%u]: [DEVICE]: %s sync terminated\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr);

	per_adv_lost = true;
}

static void recv_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info,
		    struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char data_str[129];

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
	bin2hex(buf->data, buf->len, data_str, sizeof(data_str));

	LOG_DBG("PER_ADV_SYNC[%u]: [DEVICE]: %s, tx_power %i, "
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
	LOG_DBG("BIG INFO[%u]: [DEVICE]: %s, sid 0x%02x, "
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

static uint8_t read_next_byte(struct net_buf *buf, uint8_t **pos)
{
	uint8_t byte;
	uint8_t * byte_ptr = *pos;
	byte = *(byte_ptr);
	*pos = byte_ptr + 1;
	return byte;
}

static inline uint16_t calculate_timestamp(uint16_t waited_time_sum, uint8_t timestamp)
{
	uint16_t delta;
	delta = (timestamp * (big_interval*1000)) / 127;

	return (uint16_t)delta - waited_time_sum;
}

static midi_msg_t * parse_chunk(struct net_buf *buf, uint8_t **pos, uint16_t waited_time_sum, uint8_t *end)
{	
	uint8_t *msg_start;
	uint8_t timestamp;
	uint8_t statusbyte;
	uint8_t databyte;
	uint8_t msg_len = 0;

	timestamp = read_next_byte(buf, pos);
	msg_start = *pos;
	statusbyte = read_next_byte(buf, pos);
	switch (statusbyte)
	{
	case MIDI_SYSEX_START:
		if (end == NULL)
		{
			end = net_buf_tail(buf);
		}
		while (*pos < end)
		{
			databyte = read_next_byte(buf, pos);
			if (databyte == MIDI_SYSEX_END)
			{
				break;
			}
		}
		msg_len = *pos - msg_start;
		
		break;
	
	default:
		break;
	}
	
	return midi_msg_init(buf, msg_start, msg_len, MIDI_FORMAT_1_0_PARSED_DELTA_US,
						NULL, calculate_timestamp(waited_time_sum, (127 & timestamp)), 
						0, 0, 0);
}

static midi_msg_t * parse_ump(struct net_buf *buf, uint8_t **pos, uint16_t waited_time_sum, uint8_t muid)
{
	uint8_t *msg_start;
	uint8_t timestamp;
	uint8_t msg_num;
	uint8_t ack_channel;
	uint8_t message_type;

	timestamp = read_next_byte(buf, pos);
	msg_num = read_next_byte(buf, pos);
	ack_channel = read_next_byte(buf, pos);
	msg_start = *pos;
	message_type = read_next_byte(buf, pos);

	if ((message_type >> 4) == MIDI_UMP_MSG_MIDI_1_0_CHANNEL_VOICE) {
		*pos += 3;
		if (muid == (muid_subscribe & 0xFF))
		{
			return midi_msg_init(buf, msg_start, 4, MIDI_FORMAT_2_0_UMP,
							NULL, calculate_timestamp(waited_time_sum, (127 & timestamp)),
							0, msg_num, ack_channel);
		}
	}

	return NULL;
}

static void iso_recv(struct bt_iso_chan *chan, const struct bt_iso_recv_info *info,
		struct net_buf *buf)
{
	uint8_t *buf_tail;
	uint8_t *chunk_tail;
	uint8_t *pos;

    uint8_t chunk_len;
	uint16_t waited_time_sum = 0;
    midi_msg_t * parsed_msg;
	uint8_t muid;
	uint8_t current_msg_num = 0;
	bool ump_received = false; 
	

	if (buf->len > 1) {
		LOG_HEXDUMP_INF(buf->data, buf->len, "ISO PDU");

		buf_tail = net_buf_tail(buf) - 1;
		pos = buf->data;
		while(pos < buf_tail) {

			muid = read_next_byte(buf, &pos);

			if (muid == 0xFF) {
				chunk_len = read_next_byte(buf, &pos);
				pos += 2;
				chunk_tail = pos + chunk_len;

				while (pos < chunk_tail)
				{
					parsed_msg = parse_chunk(buf, &pos, waited_time_sum, chunk_tail);
					if (parsed_msg) {

						waited_time_sum += parsed_msg->timestamp;
						
						if(iso_dev_data->api->midi_transfer_done)  {
							iso_dev_data->api->midi_transfer_done(
									iso_dev_data->dev, parsed_msg, iso_dev_data->user_data);
						} else {
							midi_msg_unref(parsed_msg);
						}
					}
				}
			} else {
				parsed_msg = parse_ump(buf, &pos, waited_time_sum, muid);
				if (parsed_msg) {
					waited_time_sum += (parsed_msg->timestamp);
					LOG_INF("msg_num: %x, previous_msg_num: %x", parsed_msg->num, previous_msg_num);
					if ((parsed_msg->num == previous_msg_num) || (parsed_msg->num == previous_msg_num - 1))
					{
						LOG_INF("Received duplicate message. Discarding.");
						parsed_msg->timestamp = 0xFF;
					} else {
						ump_received = true;
						current_msg_num = parsed_msg->num;
					}
					

					if(iso_dev_data->api->midi_transfer_done)  {
						iso_dev_data->api->midi_transfer_done(
								iso_dev_data->dev, parsed_msg, iso_dev_data->user_data);
					} else {
						midi_msg_unref_alt(parsed_msg);
					}
				}
			}
		}
	}

	if (ump_received) 
	{	
		previous_msg_num = current_msg_num;
	}
	previous_seq_num = info->seq_num;
	iso_recv_count++;
}

static void iso_connected(struct bt_iso_chan *chan)
{
	LOG_INF("ISO Channel %p connected\n", chan);
}

static void iso_disconnected(struct bt_iso_chan *chan, uint8_t reason)
{
	LOG_INF("ISO Channel %p disconnected with reason 0x%02x\n",
	       chan, reason);

	if (reason != BT_HCI_ERR_OP_CANCELLED_BY_HOST) {
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
