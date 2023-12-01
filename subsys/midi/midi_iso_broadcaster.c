/**
 * @file
 * @brief MIDI iso broadcaster driver
 *
 * Driver for MIDI iso broadcaster
 */
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include "midi_iso_internal.h"
#include <lll_adv_iso.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/iso.h>

#include "midi/midi.h"

#include <zephyr/sys/util.h>

#include <zephyr/device.h>

#include "test_gpio_out.h"

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_iso_broadcaster
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define BUF_ALLOC_TIMEOUT (5) /* milliseconds */
#define BIG_SDU_INTERVAL_US (5000)

static K_FIFO_DEFINE(fifo_tx_data);
NET_BUF_POOL_FIXED_DEFINE(bis_tx_pool, 2,
			  BT_ISO_SDU_BUF_SIZE(CONFIG_BT_ISO_TX_MTU),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

static K_THREAD_STACK_DEFINE(iso_work_q_stack_area, 1024);

static K_SEM_DEFINE(sem_big_cmplt, 0, 1);

static struct k_work_delayable iso_dummy_work;
static struct k_work_q iso_work_q;

struct midi_iso_broadcaster_dev_data *iso_dev_data;

static uint16_t seq_num;

struct midi_iso_broadcaster_dev_data {
	struct midi_api *api;
	const struct device *dev;
	void *user_data;
};

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
	.latency = 5, /* in milliseconds */
	.packing = 0, /* 0 - sequential, 1 - interleaved */
	.framing = 0, /* 0 - unframed, 1 - framed */
};

int midi_iso_broadcaster_port_callback_set(const struct device *dev,
				 midi_transfer cb,
				 void *user_data)
{
	struct midi_iso_broadcaster_dev_data *iso_dev_data = dev->data;

	if(iso_dev_data) {
		iso_dev_data->api->midi_transfer_done = cb;
		iso_dev_data->user_data = user_data;
		return 0;
	}

	return -ENOTSUP;
}

static inline uint8_t calculate_timestamp(int64_t uptime, int64_t reftime)
{
	int64_t delta;
	uint8_t timestamp;

	if(uptime <= reftime) {
		timestamp = 0;
	} else {
		delta = uptime - reftime;
		timestamp = (uint8_t)((delta*127)/BIG_SDU_INTERVAL_US);
	}
	
	return timestamp;
}

static int  send_to_iso_broadcaster_port(const struct device *dev,
							midi_msg_t *msg,
							void *user_data)
{
	msg->uptime = k_ticks_to_us_floor64(k_uptime_ticks());
    k_fifo_put(&fifo_tx_data, msg);

	return 0;
}

static void iso_dummy_work_handler(struct k_work *item)
{
    int ret;
	struct net_buf *dummy_buf;
	uint8_t dummy;

    dummy_buf = net_buf_alloc(&bis_tx_pool, K_MSEC(BUF_ALLOC_TIMEOUT));
	if (!dummy_buf) {
		LOG_ERR("Data buffer allocate timeout on channel isr");
	}

	dummy = 0;
	net_buf_reserve(dummy_buf, BT_ISO_CHAN_SEND_RESERVE);
	net_buf_add_mem(dummy_buf, &dummy, sizeof(dummy));

	 ret = bt_iso_chan_send(&bis_iso_chan[0], dummy_buf,
					    seq_num, BT_ISO_TIMESTAMP_NONE);
        if (ret < 0) {
            LOG_ERR("Unable to broadcast data on"
                    " : %d", ret);
            net_buf_unref(dummy_buf);
            return;
        }
        seq_num++;
}

static int radio_pdu_handler(uint8_t *payload)
{	
	uint8_t *payload_ptr;
	uint8_t *len_field;
	midi_msg_t *msg;
	uint64_t ref_time;
	uint8_t len;

	k_work_schedule_for_queue(&iso_work_q, &iso_dummy_work, 
        K_USEC(big_create_param.interval-1500));

	if (payload == NULL) {
		return 0;
	}

	// memcpy(payload, current_data, CONFIG_BT_ISO_TX_MTU);

	ref_time = k_ticks_to_us_floor64(k_uptime_ticks()) - 
			big_create_param.interval;

	payload_ptr = payload;
	len_field = payload_ptr;
	payload_ptr ++;
	len = 0;

	do {
		msg = k_fifo_get(&fifo_tx_data, K_NO_WAIT);
		if(msg) {

			msg->timestamp = (0x80 | calculate_timestamp(msg->uptime, ref_time));

			memcpy(payload_ptr, &msg->timestamp, 1);
			payload_ptr ++;
			len ++;

			memcpy(payload_ptr, msg->data, msg->len);
			len += msg->len;
			payload_ptr += msg->len;

			if(iso_dev_data->api->midi_transfer_done) {
				iso_dev_data->api->midi_transfer_done(iso_dev_data->dev, msg, iso_dev_data->user_data);
			} else {
				midi_msg_unref(msg);
			}
		}
	} while(msg);
	
	if(len > 0) {
		memset(len_field, len, 1);
	}
	return len;
}

static int midi_iso_broadcaster_device_init(const struct device *dev)
{
	int err;
    struct bt_le_ext_adv *adv;
    struct bt_iso_big *big;

    LOG_INF("INIT ISO PORT");

	iso_dev_data = dev->data;

	iso_dev_data->dev = dev;
	iso_dev_data->api = (struct midi_api*)dev->api;

	#ifndef NRF5340_XXAA_APPLICATION
    	lll_adv_iso_radio_pdu_cb_set(radio_pdu_handler);
	#endif

    k_work_queue_start(&iso_work_q, iso_work_q_stack_area,
            K_THREAD_STACK_SIZEOF(iso_work_q_stack_area), -2,
            NULL);

    k_work_init_delayable(&iso_dummy_work, iso_dummy_work_handler);

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth unable to initialize (err: %d)", err);
		return 0;
	}

    err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN_NAME, NULL, &adv);
	if (err) {
		LOG_ERR("Failed to create advertising set (err %d)", err);
		return 0;
	}
	
    err = bt_le_per_adv_set_param(adv, BT_LE_PER_ADV_DEFAULT);
	if (err) {
		LOG_ERR("Failed to set periodic advertising parameters"
		       " (err %d)", err);
		return 0;
	}

    err = bt_le_per_adv_start(adv);
	if (err) {
		LOG_ERR("Failed to enable periodic advertising (err %d)", err);
		return 0;
	}

    err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		LOG_ERR("Failed to start extended advertising (err %d)", err);
		return 0;
	}
	
    err = bt_iso_big_create(adv, &big_create_param, &big);
	if (err) {
		LOG_ERR("Failed to create BIG (err %d)", err);
		return 0;
	}

	err = k_sem_take(&sem_big_cmplt, K_FOREVER);
	if (err) {
		LOG_ERR("failed (err %d)", err);
		return 0;
	}
    LOG_INF("INIT ISO PORT DONE");

	return 0;
}

#define DEFINE_MIDI_ISO_BROADCASTER_DEV_DATA(dev)				        \
    static struct midi_iso_broadcaster_dev_data midi_iso_broadcaster_dev_data_##dev;    \

#define DEFINE_MIDI_BROADCASTER_DEVICE(dev)					  			\
	static struct midi_api midi_iso_broadcaster_api_##dev = {			\
        .midi_transfer = send_to_iso_broadcaster_port,					\
		.midi_callback_set = midi_iso_broadcaster_port_callback_set,	\
	};															\
	DEVICE_DT_DEFINE(MIDI_ISO_BROADCASTER_DEV_N_ID(dev),			  			\
			    &midi_iso_broadcaster_device_init,			  			\
			    NULL,					  						\
			    &midi_iso_broadcaster_dev_data_##dev,			  		\
			    NULL, APPLICATION,	  							\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  		\
			    &midi_iso_broadcaster_api_##dev);

#define MIDI_ISO_BROADCASTER_DEVICE(dev, _) \
	COND_CODE_1(UTIL_AND(DT_NODE_HAS_COMPAT(RADIO_DEV_N_ID(dev),\
		COMPAT_RADIO_DEVICE),                                               \
        DT_NODE_HAS_STATUS(RADIO_DEV_N_ID(dev), okay)),(        \
	DEFINE_MIDI_ISO_BROADCASTER_DEV_DATA(dev)                               \
	COND_NODE_HAS_COMPAT_CHILD(MIDI_ISO_DEV_N_ID(dev),             \
		COMPAT_MIDI_ISO_BROADCASTER_DEVICE,                                 \
		(DEFINE_MIDI_BROADCASTER_DEVICE(dev)), ())), ())\
        

LISTIFY(MIDI_ISO_DEVICE_COUNT, MIDI_ISO_BROADCASTER_DEVICE, ());