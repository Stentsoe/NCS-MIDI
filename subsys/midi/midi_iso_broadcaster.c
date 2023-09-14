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

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_iso_broadcaster
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define BUF_ALLOC_TIMEOUT (10) /* milliseconds */
#define BIG_SDU_INTERVAL_US (5000)

static K_FIFO_DEFINE(fifo_tx_data);
static K_FIFO_DEFINE(fifo_buf);
NET_BUF_POOL_FIXED_DEFINE(bis_tx_pool, 2,
			  BT_ISO_SDU_BUF_SIZE(CONFIG_BT_ISO_TX_MTU),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

static K_THREAD_STACK_DEFINE(iso_tx_work_q_stack_area, 512);
static K_SEM_DEFINE(sem_big_cmplt, 0, 1);
static K_SEM_DEFINE(sem_new_packet_create, 0, 1);

static struct k_work_delayable iso_tx_work;
static struct k_work_q iso_tx_work_q;
static struct k_work iso_encode_packet_work;

struct midi_iso_broadcaster_dev_data *iso_dev_data;

uint8_t new_data_len;
struct net_buf *current_buf;
struct net_buf *previous_buf;
uint64_t ref_time = 0;

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
	.latency = 10, /* in milliseconds */
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
	int err;

	msg->uptime = k_ticks_to_us_floor64(k_uptime_ticks());
    k_fifo_put(&fifo_tx_data, msg);
    err = k_work_submit_to_queue(&iso_tx_work_q, &iso_encode_packet_work);

	return err;
}

static void iso_tx_work_handler(struct k_work *item)
{
    int ret;

    if (!current_buf) {
       LOG_ERR("No buffer allocated");
    } else {
        
        net_buf_add_u8(current_buf, new_data_len);
        LOG_HEXDUMP_INF(current_buf->data, current_buf->len, "sending:");
        ret = bt_iso_chan_send(&bis_iso_chan[0], current_buf,
					    seq_num, BT_ISO_TIMESTAMP_NONE);
        if (ret < 0) {
            LOG_ERR("Unable to broadcast data on"
                    " : %d", ret);
            net_buf_unref(current_buf);
            return;
        }
        seq_num++;
        previous_buf = net_buf_ref(current_buf);
        new_data_len = 0;
    }
    k_sem_give(&sem_new_packet_create);

}

static void iso_encode_packet_work_handler(struct k_work *item)
{
    midi_msg_t *msg;
    msg = k_fifo_get(&fifo_tx_data, K_NO_WAIT);

    if (msg) {
        msg->timestamp = calculate_timestamp(msg->uptime, ref_time);
        net_buf_add_u8(current_buf, (0x80 | (uint8_t)msg->timestamp));
        net_buf_add_mem(current_buf, msg->data, msg->len);
        new_data_len += msg->len + 1;
        if(iso_dev_data->api->midi_transfer_done) {
            iso_dev_data->api->midi_transfer_done(iso_dev_data->dev, msg, iso_dev_data->user_data);
        } else {
            midi_msg_unref(msg);
        }
    }
}

static void radio_isr_handler(void)
{
    k_work_schedule_for_queue(&iso_tx_work_q, &iso_tx_work, 
        K_USEC(big_create_param.interval-500));
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


    lll_adv_iso_radio_isr_cb_set(radio_isr_handler);

    k_work_queue_start(&iso_tx_work_q, iso_tx_work_q_stack_area,
            K_THREAD_STACK_SIZEOF(iso_tx_work_q_stack_area), -2,
            NULL);

    k_work_init_delayable(&iso_tx_work, iso_tx_work_handler);
	k_work_init(&iso_encode_packet_work, iso_encode_packet_work_handler);


	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth unable to initialize (err: %d)", err);
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



void midi_iso_tx_thread(struct midi_iso_broadcaster_dev_data *iso_dev_data)
{
    uint8_t previous_len;

    for(;;) 
    {
        k_sem_take(&sem_new_packet_create, K_FOREVER);

        ref_time = k_ticks_to_us_floor64(k_uptime_ticks());

        current_buf = net_buf_alloc(&bis_tx_pool, K_MSEC(BUF_ALLOC_TIMEOUT));
        if (!current_buf) {
            LOG_ERR("Data buffer allocate timeout on channel isr");
        }
		net_buf_reserve(current_buf, BT_ISO_CHAN_SEND_RESERVE);

        if(previous_buf) {
            if(previous_buf->len) {
                previous_len = net_buf_remove_u8(previous_buf);
                net_buf_add_mem(current_buf, net_buf_remove_mem(previous_buf, previous_len), previous_len);
            }
            net_buf_unref(previous_buf);
        }
    }
    
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

#define MIDI_ISO_TX_THREAD_DEFINE(dev) \
    	K_THREAD_DEFINE(midi_iso_tx_thread_##dev, 1024, 						\
		midi_iso_tx_thread, &midi_iso_broadcaster_dev_data_##dev, NULL, NULL, -3, 0, 0);


#define MIDI_ISO_BROADCASTER_DEVICE(dev, _) \
	COND_CODE_1(UTIL_AND(DT_NODE_HAS_COMPAT(RADIO_DEV_N_ID(dev),\
		COMPAT_RADIO_DEVICE),                                               \
        DT_NODE_HAS_STATUS(RADIO_DEV_N_ID(dev), okay)),(        \
	DEFINE_MIDI_ISO_BROADCASTER_DEV_DATA(dev)                               \
	COND_NODE_HAS_COMPAT_CHILD(MIDI_ISO_DEV_N_ID(dev),             \
		COMPAT_MIDI_ISO_BROADCASTER_DEVICE,                                 \
		(DEFINE_MIDI_BROADCASTER_DEVICE(dev)), ())), ())\
        MIDI_ISO_TX_THREAD_DEFINE(dev)

LISTIFY(MIDI_ISO_DEVICE_COUNT, MIDI_ISO_BROADCASTER_DEVICE, ());

void print_test()
{
	LOG_INF(STRINGIFY());
}
