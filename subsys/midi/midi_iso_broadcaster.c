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
#include "midi/midi_iso.h"

#include <zephyr/sys/util.h>
#include <zephyr/device.h>

#include <nrfx_timer.h>

#include <test_gpio.h>
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_iso_broadcaster
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define BUF_ALLOC_TIMEOUT (5) /* milliseconds */
#define BIG_SDU_INTERVAL_US (5000)

static nrfx_timer_t timer = NRFX_TIMER_INSTANCE(3);

static K_FIFO_DEFINE(fifo_tx_data);
NET_BUF_POOL_FIXED_DEFINE(bis_tx_pool, 2,
			  BT_ISO_SDU_BUF_SIZE(CONFIG_BT_ISO_TX_MTU),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

static K_THREAD_STACK_DEFINE(iso_work_q_stack_area, 1024);

static void packet_thread_fn(void *p1, void *p2, void *p3);

K_THREAD_DEFINE(packet_thread, 512,                                
		packet_thread_fn, NULL, NULL, NULL,
		K_PRIO_PREEMPT(1), 0, 0);


static K_SEM_DEFINE(sem_big_cmplt, 0, 1);
static K_SEM_DEFINE(sem_overflow_ctrl, 0, 1);

static struct k_work_delayable iso_dummy_work;
static struct k_work_delayable iso_resend_work;
static struct k_work_q iso_work_q;

struct midi_iso_broadcaster_dev_data *iso_dev_data;

midi_ump_function_block_t *midi_ump_func_block;

static uint16_t seq_num;
static uint8_t msg_num = 0;

uint8_t ack_channels[5] = {0, 0, 0, 0, 0};

struct payload_construction {
	uint8_t *ptr;
	uint8_t *start_ptr;
	uint8_t *next_ptr;
	uint8_t *len_field;
	uint64_t ref_time;
	enum midi_format previous_format;
	uint8_t len;
};

struct payload_construction payload_constructor;

struct midi_iso_broadcaster_dev_data {
	struct midi_api *api;
	const struct device *dev;
	void *user_data;
};

uint8_t * iso_tx_msg_list_resend(uint8_t *payload_ptr);
void add_msg_to_payload(midi_msg_t *msg);

void midi_iso_set_function_block(midi_ump_function_block_t *function_block)
{
	midi_ump_func_block = function_block;
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
	msg->num = 0xFF;
	
    k_fifo_put(&fifo_tx_data, msg);

	return 0;
}

static void iso_dummy_work_handler(struct k_work *item)
{
	iso_tx_msg_list_resend(NULL);
}

static int assign_ack_channel(uint8_t *ack_channels, uint8_t num_channels, void *muid)
{
	for (int i = 0; i < num_channels; i++)
	{
		if (ack_channels[i] == 0 ) {
			ack_channels[i] = *(uint8_t *)muid;
			return i;
		}

		if (ack_channels[i] == *(uint8_t *)muid){
			return 0xFF;
		}
	}

	return 0xFF;
}

static void iso_tx_msg_add(midi_msg_t *msg)
{
	midi_ump_tx_msg_t *tx_msg = k_malloc(sizeof(midi_ump_tx_msg_t));

	if (tx_msg) {
		msg->num = msg_num;
		tx_msg->msg = msg;
		sys_slist_append(&midi_ump_func_block->tx_msg_list, &tx_msg->node);
	}
}

void iso_tx_msg_list_print(void)
{
	midi_ump_tx_msg_t *tx_msg;
	sys_slist_t *msg_list = &midi_ump_func_block->tx_msg_list;
    SYS_SLIST_FOR_EACH_CONTAINER(msg_list, tx_msg, node) {
		LOG_INF("num: %x", tx_msg->msg->num);
    }
    return;
}

bool iso_tx_msg_remove(uint8_t muid, uint8_t num)
{
	midi_ump_tx_msg_t *tx_msg;
	sys_snode_t *prev_node = NULL;
	sys_slist_t *msg_list = &midi_ump_func_block->tx_msg_list;
    SYS_SLIST_FOR_EACH_CONTAINER(msg_list, tx_msg, node) {
        if (tx_msg->msg->num == num) {
			if (*(uint8_t *)tx_msg->msg->context == muid)
			{
				sys_slist_remove(msg_list, prev_node, &tx_msg->node);
				if(iso_dev_data->api->midi_transfer_done) {
					iso_dev_data->api->midi_transfer_done(iso_dev_data->dev, tx_msg->msg, iso_dev_data->user_data);
				} else {
					midi_msg_unref(tx_msg->msg);
				}
				k_free(tx_msg);
				return true;
			}
        }
		prev_node = &tx_msg->node;
    }

    return false;
}

uint8_t * iso_tx_msg_list_resend(uint8_t *payload_ptr)
{
	midi_ump_tx_msg_t *tx_msg;
	sys_slist_t *msg_list = &midi_ump_func_block->tx_msg_list;
	uint8_t count = 0;

	if (!sys_slist_is_empty(msg_list))
	{
		SYS_SLIST_FOR_EACH_CONTAINER(msg_list, tx_msg, node) {
			if (tx_msg->msg->num < msg_num)
			{
				k_fifo_put(&fifo_tx_data, tx_msg->msg);
				count++;
			}
		}
	}

	if(count > 0)
	{
		LOG_DBG("Resending %d", count);
	}
	
    return 0;
}

void midi_iso_ack_msg(uint8_t muid, uint8_t msg_num)
{
	while(iso_tx_msg_remove(muid, msg_num)) {}
}

void add_msg_to_payload(midi_msg_t *msg)
{
	uint8_t *payload_ptr;
	uint8_t *len_field;
	enum midi_format previous_format;
	uint64_t ref_time;
	uint8_t len;

	int lock = irq_lock();

	payload_ptr = payload_constructor.ptr;
	len_field = payload_constructor.len_field;
	previous_format = payload_constructor.previous_format;
	ref_time = payload_constructor.ref_time;

	if (msg->format == MIDI_FORMAT_1_0_PARSED) {
		if (previous_format != MIDI_FORMAT_1_0_PARSED) {
			memset(payload_ptr, 0xFF, 1);
			payload_ptr++;
			len_field = payload_ptr;
			memset(payload_ptr, 0, 3);
			payload_ptr += 3;
		}
		payload_constructor.previous_format = MIDI_FORMAT_1_0_PARSED;
		msg->timestamp = (0x80 | calculate_timestamp(msg->uptime, ref_time));
		memcpy(payload_ptr, &msg->timestamp, 1);
		payload_ptr ++;
		memcpy(payload_ptr, msg->data, msg->len);
		payload_ptr += msg->len;
		len = *len_field;
		len += (msg->len + 1);
		memset(len_field, len, 1);
	}

	else if (msg->format == MIDI_FORMAT_2_0_UMP)
	{
		payload_constructor.previous_format = MIDI_FORMAT_2_0_UMP;
		memcpy(payload_ptr, msg->context, 1);
		payload_ptr ++;
		msg->timestamp = (0x80 | calculate_timestamp(msg->uptime, ref_time));
		memcpy(payload_ptr, &msg->timestamp, 1);
		payload_ptr ++;
		if(msg->num == 0xFF)
		{
			memset(payload_ptr, msg_num, 1);
			iso_tx_msg_add(msg);
		} else {
			memset(payload_ptr, msg->num, 1);
		}
		
		payload_ptr ++;
		memset(payload_ptr, assign_ack_channel(ack_channels, 5, msg->context), 1);
		uint8_t ack_channel = *payload_ptr;

		payload_ptr ++;
		memcpy(payload_ptr, msg->data, msg->len);
		payload_ptr += msg->len;
	}

	payload_constructor.ptr = payload_ptr;
	payload_constructor.len_field = len_field;

	irq_unlock(lock);
}

static void packet_thread_fn(void *p1, void *p2, void *p3)
{
	midi_msg_t *msg;
	uint8_t len;

	while(1)
	{
		len = payload_constructor.ptr - payload_constructor.start_ptr;

		msg = k_fifo_get(&fifo_tx_data, K_FOREVER);
		if(msg) {
			if ((len + msg->len + 5) > (CONFIG_BT_CTLR_ADV_ISO_PDU_LEN_MAX - 3))
			{
				k_sem_take(&sem_overflow_ctrl, K_FOREVER);
			}
			add_msg_to_payload(msg);
		}
	}
}

static int next_pdu_handler(uint8_t *payload)
{
	payload_constructor.next_ptr = payload + 3;	

	return 0;
}

static int radio_pdu_handler(uint8_t *payload)
{	
	uint8_t len;
	
	memset(ack_channels, 0, 5);
	k_work_schedule_for_queue(&iso_work_q, &iso_dummy_work, 
        K_USEC(big_create_param.interval-1500));

	nrfx_timer_clear(&timer);
	nrfx_timer_compare(&timer, NRF_TIMER_CC_CHANNEL1, nrfx_timer_us_to_ticks(&timer, 3500), true);

	if (payload == NULL) {
		return 0;
	}

	payload_constructor.ref_time = k_ticks_to_us_floor64(k_uptime_ticks());
	payload_constructor.previous_format = MIDI_FORMAT_2_0_UMP;
	
	len = payload_constructor.ptr - payload;

	payload_constructor.ptr = payload_constructor.next_ptr;
	payload_constructor.start_ptr = payload_constructor.ptr;

	k_sem_give(&sem_overflow_ctrl);

	if (len > 0)
	{
		msg_num++;
		if (msg_num == 0xFF)
		{
			msg_num = 0;
		}

		if (len > 88) {
			LOG_WRN("Packet length is excessive  %d", len);
		} else {
			LOG_HEXDUMP_INF(payload, len, "TX:");
		}
	}

	return len;
}

static void timer_handler(nrf_timer_event_t event_type, void *context)
{
	if (event_type == NRF_TIMER_EVENT_COMPARE1) 
	{
		nrf_timer_event_clear(timer.p_reg, NRF_TIMER_EVENT_COMPARE1);
		
		int err;
		struct net_buf *dummy_buf;
		
		dummy_buf = net_buf_alloc(&bis_tx_pool, K_MSEC(BUF_ALLOC_TIMEOUT));
		if (!dummy_buf) {
			LOG_ERR("Data buffer allocate timeout on channel isr");
		}

		net_buf_reserve(dummy_buf, BT_ISO_CHAN_SEND_RESERVE);

		err = bt_iso_chan_send(&bis_iso_chan[0], dummy_buf,
							seq_num, BT_ISO_TIMESTAMP_NONE);
		if (err < 0) {
			LOG_ERR("Unable to broadcast data on"
					" : %d", err);
			net_buf_unref(dummy_buf);
			return;
		}
		seq_num++;
	}

	if (event_type == NRF_TIMER_EVENT_COMPARE2) 
	{
		nrf_timer_event_clear(timer.p_reg, NRF_TIMER_EVENT_COMPARE2);
	}
}

static int sys_timer_init(void)
{
	nrfx_err_t nrfx_err;
	const nrfx_timer_config_t config = {
		.frequency = NRFX_MHZ_TO_HZ(1),
		.mode = NRF_TIMER_MODE_TIMER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
	};

	nrfx_err = nrfx_timer_init(&timer, &config, timer_handler);
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize nrfx timer (err %d)", nrfx_err);
		return -EFAULT;
	}

	IRQ_CONNECT(NRFX_CONCAT_3(TIMER, 3, _IRQn), 3,
		    NRFX_CONCAT_3(nrfx_timer_, 3, _irq_handler), NULL, 0);

	nrfx_timer_enable(&timer);
	return 0;
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

	lll_adv_iso_radio_pdu_cb_set(radio_pdu_handler);
	lll_adv_iso_radio_next_pdu_cb_set(next_pdu_handler);

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

	err = sys_timer_init();
	if (err) {
		LOG_ERR("Cannot init timer (err: %d)", err);
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