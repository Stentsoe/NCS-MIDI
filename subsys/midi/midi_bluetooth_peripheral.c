/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MIDI bluetooth driver
 *
 * Driver for MIDI bluetooth
 */
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include "midi_bluetooth_internal.h"

#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <sdc_hci_vs.h>

#include <bluetooth/services/midi.h>

#include "midi/midi.h"
#include "midi/midi_types.h"
#include "midi/midi_bluetooth.h"

#include <mpsl_radio_notification.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_bluetooth_peripheral
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RADIO_NOTIF_PRIORITY 1
#define BLE_MIDI_TX_MAX_SIZE 73

#define INTERVAL_LLPM 0x0D01 /* Proprietary  1 ms */
#define INTERVAL_LLPM_US 1000

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static K_FIFO_DEFINE(fifo_tx_data);

static K_THREAD_STACK_DEFINE(ble_tx_work_q_stack_area, 512);

struct midi_bluetooth_dev_data *midi_bluetooth_device_data;

static struct bt_conn *current_conn;

static struct k_work ble_tx_work;
static struct k_work_q ble_tx_work_q;
static struct k_work ble_midi_encode_work;

static uint8_t ble_midi_pck[BLE_MIDI_TX_MAX_SIZE];
static uint8_t ble_midi_pck_len;
static uint8_t mtu_size = BLE_MIDI_TX_MAX_SIZE + 3;

static bool radio_notif_flag;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_MIDI_VAL),
};

struct midi_bluetooth_api {
	int (*connected)(struct bt_conn *conn, 
                                    uint8_t conn_err);
};


struct midi_bluetooth_api midi_bluetooth_callbacks;

struct midi_bluetooth_in_dev_data {

	struct midi_api *api;

	const struct device *dev;

	void *user_data;

};

struct midi_bluetooth_out_dev_data {

	struct midi_api *api;

	const struct device *dev;

	void *user_data;
};

struct midi_bluetooth_dev_data {

	struct midi_bluetooth_in_dev_data *in;

	struct midi_bluetooth_out_dev_data *out;

};

int midi_bluetooth_register_connected_cb(midi_bluetooth_connected cb)
{
	midi_bluetooth_callbacks.connected = cb;
	return 0;
}

int midi_bluetooth_advertise(const struct device *dev)
{
	int err;
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
			      ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
	}
	LOG_INF("Advertising successfully started");
	return err;
}

int midi_bluetooth_out_port_callback_set(const struct device *dev,
				 midi_transfer cb,
				 void *user_data)
{
	struct midi_bluetooth_dev_data *bluetooth_dev_data = dev->data;


	if(bluetooth_dev_data->out) {
		bluetooth_dev_data->out->api->midi_transfer_done = cb;
		bluetooth_dev_data->out->user_data = user_data;
		return 0;
	}

	return -ENOTSUP;
}

int midi_bluetooth_in_port_callback_set(const struct device *dev,
				 midi_transfer cb,
				 void *user_data)
{
	struct midi_bluetooth_dev_data *bluetooth_dev_data = dev->data;

	if(bluetooth_dev_data->in) {
		bluetooth_dev_data->in->api->midi_transfer_done = cb;
		bluetooth_dev_data->in->user_data = user_data;
		return 0;
	}

	return -ENOTSUP;
}

static int  send_to_bluetooth_port(const struct device *dev,
							midi_msg_t *msg,
							void *user_data)
{
	int err;
	
	struct midi_bluetooth_out_dev_data *out;
	out = midi_bluetooth_device_data->out;
	
	if (current_conn) {
		// LOG_INF("skjer1");
		k_fifo_put(&fifo_tx_data, msg);
		err = k_work_submit_to_queue(&ble_tx_work_q, &ble_midi_encode_work);
	} else {
		if(out->api->midi_transfer_done) {
			out->api->midi_transfer_done(out->dev, msg, out->user_data);
		} else {
			midi_msg_unref(msg);
		}
		err = -ENOTCONN;
	}
	
	return err;
}

static void ble_tx_work_handler(struct k_work *item)
{
	if (ble_midi_pck_len != 0) {
		// LOG_INF("skjer tx handler");
		bt_midi_send(current_conn, ble_midi_pck, ble_midi_pck_len);
		// LOG_INF("skjer tx handler2");
		ble_midi_pck_len = 0;
	}
}

static void ble_midi_encode_work_handler(struct k_work *item)
{
	midi_msg_t *msg;
	static uint8_t running_status = 0;
	struct midi_bluetooth_out_dev_data *out;
	out = midi_bluetooth_device_data->out;
	// LOG_INF("skjer2");
	msg = k_fifo_get(&fifo_tx_data, K_NO_WAIT);
	/** Process received MIDI message and prepare BLE packet */
	if (msg) {
		if ((msg->len + 1) > (mtu_size - 3) - ble_midi_pck_len) {
			/** BLE Packet is full, packet is ready to send */
			bt_midi_send(current_conn, ble_midi_pck,
				     ble_midi_pck_len);
			ble_midi_pck_len = 0;
		}

		if (ble_midi_pck_len == 0) {
			/** New packet */
			ble_midi_pck[0] = ((msg->timestamp >> 7) | 0x80) & 0xBF;
			ble_midi_pck_len = 1;
			running_status = 0;
		}

		/** Enforce Running status */
		if (msg->data[0] == running_status) {
			/** Running status */
			msg->len--;
			msg->data[0] = msg->data[1];
			msg->data[1] = msg->data[2];
		} else if (msg->data[0] < 0xF8) {
			/** Running status cancelled, new? */
			if (msg->data[0] < 0xF0) {
				running_status = msg->data[0];
			} else {
				running_status = 0;
			}
		} else {
			/** System RTM: Unchanged */
		}

		/** Message Timestamp */
		ble_midi_pck[ble_midi_pck_len] = (msg->timestamp & 0x7F) | 0x80;
		ble_midi_pck_len++;
		
		/** Add MIDI message to packet and free memory */
		LOG_INF("tx1");
		memcpy((ble_midi_pck + ble_midi_pck_len), msg->data, msg->len);
		LOG_INF("tx2");
		ble_midi_pck_len += msg->len;
		
		if(out->api->midi_transfer_done) {
			out->api->midi_transfer_done(out->dev, msg, out->user_data);
		} else {
			midi_msg_unref(msg);
		}
		// midi_msg_unref(msg);
		k_work_submit_to_queue(&ble_tx_work_q, &ble_midi_encode_work);
	}
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn_err) {
		LOG_ERR("Connection failed (err %u)", conn_err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	// LOG_INF("Connected to %d", log_strdup(addr));

	current_conn = bt_conn_ref(conn);

	/** Return uart_write_thread from k_fifo_get, disable uart echo.  */
	k_fifo_cancel_wait(&fifo_tx_data);

	k_work_init(&ble_tx_work, ble_tx_work_handler);
	k_work_init(&ble_midi_encode_work, ble_midi_encode_work_handler);

	if(midi_bluetooth_callbacks.connected) {
		midi_bluetooth_callbacks.connected(conn, conn_err);
	}

	irq_enable(TEMP_IRQn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	// LOG_INF("Disconnected: %s (reason %u)", log_strdup(addr), reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
		irq_disable(TEMP_IRQn);
	}
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param) {
	LOG_INF("Param requested");
	return true;
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
			     uint16_t latency, uint16_t timeout)
{
	struct bt_conn_info info;

	bt_conn_get_info(conn, &info);

	mtu_size = bt_gatt_get_mtu(conn);

	if (interval == INTERVAL_LLPM) {
		LOG_INF("Connection interval updated: LLPM (1 ms), MTU: %d\n",
			mtu_size);
	} else {
		if (info.le.interval > 12) {
			const struct bt_le_conn_param param = {
				.interval_min = 6,
				.interval_max = 6,
				.latency = 0,
				.timeout = info.le.timeout
			};

			bt_conn_le_param_update(conn, &param);
		}

		LOG_INF("Params updated interval: %d, latency: %d, timeout %d: MTU: %d",
			interval, latency, timeout, mtu_size);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_req = le_param_req,
	.le_param_updated = le_param_updated,
};


static uint16_t convert_timestamp(uint16_t timestamp, uint16_t conn_interval,
				  uint16_t conn_time)
{
	static uint16_t correction;

	uint16_t interval_start = TIMESTAMP(conn_time + correction);
	uint16_t interval_end = interval_start + conn_interval;

	if (timestamp > interval_end) {
		correction = TIMESTAMP(timestamp - conn_time - conn_interval);
	} else if ((timestamp < (interval_start)) &&
		   (interval_start < TIMESTAMP(interval_end))) {
		correction = TIMESTAMP(timestamp - conn_time);
	}

	return TIMESTAMP(timestamp - correction);
}

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len)
{
	midi_msg_t *rx_msg;
	struct midi_bluetooth_in_dev_data *in;
	in = midi_bluetooth_device_data->in;
	// static struct midi_parser_t parser;
	static uint32_t conn_time;

	uint8_t current_byte;
	uint16_t timestamp_ble;
	uint16_t timestamp;
	uint16_t interval;

	struct bt_conn_info conn_info;
	bt_conn_get_info(conn, &conn_info);

	if (conn_info.le.interval == INTERVAL_LLPM) {
		interval = 2;
	} else {
		interval = (conn_info.le.interval * 1.25) + 1;
	}

	bool next_is_new_timestamp = false;

	if (radio_notif_flag) {
		radio_notif_flag = false;
		conn_time = TIMESTAMP(k_ticks_to_ms_near64(k_uptime_ticks()));
	};

	char addr[BT_ADDR_LE_STR_LEN] = { 0 };
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

	/** Collecting first Timestamp */
	timestamp_ble = (*(data + 1) & 0x7F) + ((*data & 0x3F) << 7);
	timestamp = convert_timestamp(timestamp_ble, interval, conn_time);

	/** Decoding rest of data */
	for (uint8_t pos = 2; pos < len; pos++) {
		current_byte = *(data + pos);

		/** Check MSB and expectations to ID current byte as timestamp,
		 * statusbyte or databyte */
		if ((current_byte >> 7) && (next_is_new_timestamp)) {
			/** New Timestamp means last message is complete */
			next_is_new_timestamp = false;

			if ((current_byte & 0x7F) < (timestamp_ble & 0x7F)) {
				/** Timestamp overflow, increment Timestamp High */
				timestamp_ble += 1 << 7;
			}

			/** Storing newest timestamp for later reference, and translating
			 * timestamp to local time */
			timestamp_ble = ((current_byte & 0x7F) +
					 (timestamp_ble & 0x1F80));
			timestamp = convert_timestamp(timestamp_ble, interval,
						      conn_time);

			if (!rx_msg) {
				/** Previous message was not complete */
				LOG_ERR("Incomplete message: pos %d.", pos);
			}
		} else {
			/** Statusbytes and databytes */
			next_is_new_timestamp = true;

			rx_msg = midi_msg_alloc(NULL, 1);
			LOG_INF("bt_receive_cb1");
			memcpy(rx_msg->data, &current_byte, 1);
			LOG_INF("bt_receive_cb2");
			rx_msg->format = MIDI_FORMAT_1_0_SERIAL;
			rx_msg->len = 1;
			rx_msg->timestamp = timestamp;
			
			if (rx_msg) {
				if(in->api->midi_transfer_done) {
					in->api->midi_transfer_done(in->dev, rx_msg, in->user_data);
				} else {
					midi_msg_unref(rx_msg);
				}
			}

			// rx_msg = midi_parse_byte(timestamp, current_byte, &parser);

			// if (rx_msg) {
			// 	/** Message completed */
			// 	LOG_HEXDUMP_INF(rx_msg->data, rx_msg->len, "rx:");
				
			// }
		}
	}
}


void bt_sent_cb(struct bt_conn *conn) {

}

static struct bt_midi_cb midi_cb = {
	.received = bt_receive_cb,
	.sent = bt_sent_cb,
};

static void radio_notif_handler(void)
{
	radio_notif_flag = true;
	k_work_submit_to_queue(&ble_tx_work_q, &ble_tx_work);
}

static int enable_llpm_mode(void)
{
	int err;
	struct net_buf *buf;
	sdc_hci_cmd_vs_llpm_mode_set_t *cmd_enable;

	buf = bt_hci_cmd_create(SDC_HCI_OPCODE_CMD_VS_LLPM_MODE_SET,
				sizeof(*cmd_enable));
	if (!buf) {
		LOG_INF("Could not allocate LLPM command buffer\n");
		return -ENOMEM;
	}

	cmd_enable = net_buf_add(buf, sizeof(*cmd_enable));
	cmd_enable->enable = true;

	err = bt_hci_cmd_send_sync(SDC_HCI_OPCODE_CMD_VS_LLPM_MODE_SET, buf,
				   NULL);
	if (err) {
		LOG_INF("Error enabling LLPM %d\n", err);
		return err;
	}

	LOG_INF("LLPM mode enabled\n");
	return 0;
}

static int midi_bluetooth_in_device_init(const struct device *dev)
{
	int err;

	struct midi_bluetooth_dev_data *bluetooth_dev_data = dev->data;

	bluetooth_dev_data->in->dev = dev;
	bluetooth_dev_data->in->api = (struct midi_api*)dev->api;
	midi_bluetooth_device_data = bluetooth_dev_data;
	err = 0;

	return err;
}

static int midi_bluetooth_out_device_init(const struct device *dev)
{
	int err;

	struct midi_bluetooth_dev_data *bluetooth_dev_data = dev->data;

	bluetooth_dev_data->out->dev = dev;
	bluetooth_dev_data->out->api = (struct midi_api*)dev->api;



	k_work_queue_start(&ble_tx_work_q, ble_tx_work_q_stack_area,
			   K_THREAD_STACK_SIZEOF(ble_tx_work_q_stack_area), -2,
			   NULL);

	mpsl_radio_notification_cfg_set(
		MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_ACTIVE,
		MPSL_RADIO_NOTIFICATION_DISTANCE_420US, TEMP_IRQn);
	
	IRQ_CONNECT(TEMP_IRQn, RADIO_NOTIF_PRIORITY, radio_notif_handler, NULL, 0);

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth unable to initialize (err: %d)", err);
	}
	bt_conn_cb_register(&conn_callbacks);

	err = bt_midi_init(&midi_cb);
	if (err) {
		LOG_ERR("Failed to initialize MIDI service (err: %d)", err);
		return err;
	}

	if (enable_llpm_mode()) {
		LOG_ERR("Enable LLPM mode failed.\n");
		return err;
	}

	return 0;
}

#define DEFINE_MIDI_BLUETOOTH_IN_DEV_DATA(dev)										\
	static struct midi_bluetooth_in_dev_data midi_bluetooth_in_dev_data_##dev; 			

#define DEFINE_MIDI_BLUETOOTH_OUT_DEV_DATA(dev)									\
	static struct midi_bluetooth_out_dev_data midi_bluetooth_out_dev_data_##dev;																			\

#define DEFINE_MIDI_BLUETOOTH_DEV_DATA(dev)				\
	COND_NODE_HAS_COMPAT_CHILD(MIDI_BLUETOOTH_DEV_N_ID(dev), 		\
		COMPAT_MIDI_BLUETOOTH_IN_DEVICE, 					\
		(DEFINE_MIDI_BLUETOOTH_IN_DEV_DATA(dev)), ()) 		\
	COND_NODE_HAS_COMPAT_CHILD(MIDI_BLUETOOTH_DEV_N_ID(dev), 		\
		COMPAT_MIDI_BLUETOOTH_OUT_DEVICE, 					\
		(DEFINE_MIDI_BLUETOOTH_OUT_DEV_DATA(dev)), ())		\
	static struct midi_bluetooth_dev_data midi_bluetooth_dev_data_##dev = {	\
		COND_NODE_HAS_COMPAT_CHILD(MIDI_BLUETOOTH_DEV_N_ID(dev), 					\
			COMPAT_MIDI_BLUETOOTH_IN_DEVICE, 								\
			(.in = &midi_bluetooth_in_dev_data_##dev, ),				\
			(.in = NULL,)) 												\
		COND_NODE_HAS_COMPAT_CHILD(MIDI_BLUETOOTH_DEV_N_ID(dev), 					\
			COMPAT_MIDI_BLUETOOTH_OUT_DEVICE, 								\
			(.out = &midi_bluetooth_out_dev_data_##dev,), 				\
			(.out = NULL,))												\
	};

#define DEFINE_MIDI_BLUETOOTH_IN_DEVICE(dev)					  			\
	static struct midi_api midi_bluetooth_in_api_##dev = {			\
		.midi_callback_set = midi_bluetooth_in_port_callback_set,	\
	};															\
	DEVICE_DT_DEFINE(MIDI_BLUETOOTH_IN_DEV_N_ID(dev),			  			\
			    &midi_bluetooth_in_device_init,			  			\
			    NULL,					  						\
			    &midi_bluetooth_dev_data_##dev,			  		\
			    NULL, APPLICATION,	  							\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  		\
			    &midi_bluetooth_in_api_##dev);

#define DEFINE_MIDI_BLUETOOTH_OUT_DEVICE(dev)					  			\
	static struct midi_api midi_bluetooth_out_api_##dev = {			\
		.midi_transfer = send_to_bluetooth_port,					\
		.midi_callback_set = midi_bluetooth_out_port_callback_set,	\
	};															\
	DEVICE_DT_DEFINE(MIDI_BLUETOOTH_OUT_DEV_N_ID(dev),			  			\
			    &midi_bluetooth_out_device_init,			  			\
			    NULL,					  						\
			    &midi_bluetooth_dev_data_##dev,			  	\
			    NULL, APPLICATION,	  							\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  		\
			    &midi_bluetooth_out_api_##dev);

#define MIDI_BLUETOOTH_IN_DEVICE(dev)   \
	DEFINE_MIDI_BLUETOOTH_IN_DEVICE(dev)   			\

#define MIDI_BLUETOOTH_OUT_DEVICE(dev)  	\
	DEFINE_MIDI_BLUETOOTH_OUT_DEVICE(dev)   				\
	
#define MIDI_BLUETOOTH_DEVICE(dev, _) \
	COND_CODE_1(UTIL_AND(DT_NODE_HAS_COMPAT(RADIO_DEV_N_ID(dev), \
		nordic_nrf_radio), DT_NODE_HAS_STATUS(RADIO_DEV_N_ID(dev), okay)),( \
	DEFINE_MIDI_BLUETOOTH_DEV_DATA(dev)\
	COND_NODE_HAS_COMPAT_CHILD(MIDI_BLUETOOTH_DEV_N_ID(dev), \
		COMPAT_MIDI_BLUETOOTH_IN_DEVICE, \
		(MIDI_BLUETOOTH_IN_DEVICE(dev)), ()) \
	COND_NODE_HAS_COMPAT_CHILD(MIDI_BLUETOOTH_DEV_N_ID(dev), \
		COMPAT_MIDI_BLUETOOTH_OUT_DEVICE, \
		(MIDI_BLUETOOTH_OUT_DEVICE(dev)), ())), ())

LISTIFY(MIDI_DEVICE_COUNT, MIDI_BLUETOOTH_DEVICE, ());


void print_test()
{
	LOG_INF(STRINGIFY((COND_NODE_HAS_COMPAT_CHILD(MIDI_BLUETOOTH_DEV_N_ID(0), \
		COMPAT_MIDI_BLUETOOTH_IN_DEVICE, \
		(MIDI_BLUETOOTH_IN_DEVICE(0)), ()))));
}
