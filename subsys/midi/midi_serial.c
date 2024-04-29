/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MIDI serial driver
 *
 * Driver for MIDI serial
 */
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/drivers/uart.h>
#include "midi_serial_internal.h"

#include "midi/midi.h"

#include <zephyr/sys/util.h>

#include <zephyr/device.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_serial
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

enum timestamp_setting {
	MIDI_TIMESTAMP_OFF,
	MIDI_TIMESTAMP_ON
};

struct midi_serial_in_dev_data {

	struct midi_api *api;

	const struct device *dev;
	
	uint8_t rx_buffer[2];

	uint8_t *released_buf;

	struct k_fifo rx_queue;

	void *user_data;

};

struct midi_serial_out_dev_data {

	struct midi_api *api;

	const struct device *dev;

	// uint8_t tx_buffer[2];

	// uint8_t current_buf_idx;

	struct k_sem tx_sem;

	struct k_fifo tx_queue;

	enum timestamp_setting timestamp_setting;

	void *user_data;
};

struct midi_serial_dev_data {

	const struct device * uart_dev;

	struct midi_serial_in_dev_data *in;

	struct midi_serial_out_dev_data *out;

};

uint8_t buffer_1[16];
uint8_t buffer_2[16];
uint8_t *buffers[2] = {buffer_1, buffer_2};


static void uart_cb(const struct device *dev, struct uart_event *evt,
		    void *user_data)
{
	struct midi_serial_dev_data *serial_dev_data = user_data;
	struct midi_serial_in_dev_data *in = serial_dev_data->in;
	struct midi_serial_out_dev_data *out = serial_dev_data->out;
	static uint8_t *released_buf;
	if (serial_dev_data->in)
	{
		released_buf = serial_dev_data->in->released_buf;
	}
	
	midi_msg_t *msg;

	switch (evt->type) {
	case UART_TX_DONE:
		k_sem_give(&out->tx_sem);
		break;
	case UART_RX_RDY:
		// LOG_INF("UART RX-ready, len %d, %d", evt->data.rx.len, evt->data.rx.buf[evt->data.rx.offset]);
		msg = midi_msg_alloc(NULL, 1);
		memcpy(msg->data, &evt->data.rx.buf[evt->data.rx.offset], 1);
		msg->format = MIDI_FORMAT_1_0_SERIAL;
		msg->len = 1;
		msg->timestamp = MIDI_TIME_13BIT(k_ticks_to_ms_near64(k_uptime_ticks()));
		
		if (msg) {
			k_fifo_put(&in->rx_queue, msg);
		}

		break;
	case UART_RX_DISABLED:
		LOG_WRN("UART RX-disabled");
		break;
	case UART_RX_BUF_REQUEST:
	
		// LOG_INF("UART RX-request");
		// if (released_buf == &in->rx_buffer[0]) {
		// 	uart_rx_buf_rsp(serial_dev_data->uart_dev, &in->rx_buffer[0], 1);
		// } else {
		// 	uart_rx_buf_rsp(serial_dev_data->uart_dev, &in->rx_buffer[1], 1);
		// }		
		if (released_buf == buffers[0]) {
			uart_rx_buf_rsp(serial_dev_data->uart_dev, buffers[0], 16);
		} else {
			uart_rx_buf_rsp(serial_dev_data->uart_dev, buffers[1], 16);
		}
		break;
	case UART_RX_BUF_RELEASED:
	// LOG_INF("UART RX-released");
		released_buf = evt->data.rx_buf.buf;
		// uart_rx_buf_rsp(serial_dev_data->uart_dev, released_buf, 16);
		break;
	case UART_RX_STOPPED:
	// LOG_WRN("UART RX-stopped");
		LOG_WRN("UART Rx stopped, reason %d", evt->data.rx_stop.reason);
	// 	if ((evt->data.rx_stop.reason == 4) ||
	// 	    (evt->data.rx_stop.reason == 8)) {
	// 		/** UART might be disconnected, 
	// 		 * disable until rx-port is connected again. */
	// 		uart_rx_disable(uart);
	// 		k_work_schedule(&uart_rx_enable_work, K_MSEC(500));
	// 	}
		break;
	default:
		break;
	}
}

int midi_serial_out_port_callback_set(const struct device *dev,
				 midi_transfer cb,
				 void *user_data)
{
	struct midi_serial_dev_data *serial_dev_data = dev->data;


	if(serial_dev_data->out) {
		serial_dev_data->out->api->midi_transfer_done = cb;
		serial_dev_data->out->user_data = user_data;
		return 0;
	}

	return -ENOTSUP;
}

int midi_serial_in_port_callback_set(const struct device *dev,
				 midi_transfer cb,
				 void *user_data)
{
	struct midi_serial_dev_data *serial_dev_data = dev->data;

	if(serial_dev_data->in) {
		serial_dev_data->in->api->midi_transfer_done = cb;
		serial_dev_data->in->user_data = user_data;
		return 0;
	}

	return -ENOTSUP;
}

/** @brief UART Initializer */
static int midi_serial_in_device_init(const struct device *dev)
{
	int err;

	struct midi_serial_dev_data *serial_dev_data = dev->data;
	const struct device * uart_dev;
	
	serial_dev_data->in->dev = dev;
	k_fifo_init(&serial_dev_data->in->rx_queue);
	serial_dev_data->in->api = (struct midi_api*)dev->api;


	uart_dev = serial_dev_data->uart_dev;

	if (!device_is_ready(uart_dev)) {
		LOG_WRN("UART device not found!");
		return -ENXIO;
	}

	LOG_INF("INIT UART IN PORT");

	err = uart_callback_set(uart_dev, uart_cb, serial_dev_data);
	if (err) {
		return err;
	}
	err = uart_rx_enable(uart_dev, buffers[0], 16, 0);
	if (err) {
		return err;
	}

	LOG_INF("Init MIDI SERIAL IN PORT: dev %p (%s)", dev, dev->name);

	return 0;
}

/** @brief UART Initializer */
static int midi_serial_out_device_init(const struct device *dev)
{
	int err;

	struct midi_serial_dev_data *serial_dev_data = dev->data;
	const struct device * uart_dev;
	
	serial_dev_data->out->dev = dev;
	serial_dev_data->out->api = (struct midi_api*)dev->api;

	
	uart_dev = serial_dev_data->uart_dev;

	k_fifo_init(&serial_dev_data->out->tx_queue);
	k_sem_init(&serial_dev_data->out->tx_sem, 1, 1);

	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return -ENXIO;
	}

	LOG_INF("INIT UART PORT");

	err = uart_callback_set(uart_dev, uart_cb, serial_dev_data);
	if (err) {
		return err;
	}
	
	LOG_INF("Init MIDI SERIAL OUT PORT: dev %p (%s)", dev, dev->name);

	return 0;
}

void midi_rx_thread(struct midi_serial_dev_data *serial_dev_data)
{	
	midi_msg_t *msg;
	struct midi_serial_in_dev_data *in = serial_dev_data->in;

	for (;;) {
		msg = k_fifo_get(&in->rx_queue, K_FOREVER);

		
		if(in->api->midi_transfer_done) {
			in->api->midi_transfer_done(in->dev, msg, in->user_data);
		} else {
			midi_msg_unref(msg);
		}
	}
}

static int send_to_serial_port(const struct device *dev,
				    midi_msg_t *msg,
					void *user_data)
{
	struct midi_serial_dev_data *serial_dev_data = dev->data;
	struct midi_serial_out_dev_data *out = serial_dev_data->out;

	if((msg->format != MIDI_FORMAT_1_0_SERIAL) &
	   (msg->format != MIDI_FORMAT_1_0_PARSED) &
	   (msg->format != MIDI_FORMAT_1_0_PARSED_DELTA_US)) {
		LOG_WRN("Tried to send wrong format on serial port. format: %d", msg->format);
		return -EINVAL;
	}

	if(!out) {
		return -ENOTSUP;
	}

	k_fifo_put(&out->tx_queue, msg);
	return 0;

}

void midi_tx_thread(struct midi_serial_dev_data *serial_dev_data)
{
	int32_t msg_delay;
	struct midi_serial_out_dev_data * out = serial_dev_data->out;
	midi_msg_t *msg = NULL;

	for (;;) {
		/* Wait indefinitely for data to be sent over bluetooth NOTE: ???*/
		k_sem_take(&out->tx_sem, K_FOREVER);
		if (msg) {
			if(out->api->midi_transfer_done) {
				out->api->midi_transfer_done(out->dev, msg, out->user_data);
			} else {
				midi_msg_unref(msg);
			}
		}
		msg = k_fifo_get(&out->tx_queue, K_FOREVER);
		if (out->timestamp_setting == MIDI_TIMESTAMP_ON)
		{
			if (msg->format == MIDI_FORMAT_1_0_PARSED_DELTA_US) {
				k_sleep(K_USEC(msg->timestamp));
			} else {
				msg_delay = (int32_t)((msg->timestamp) -
					(k_ticks_to_ms_near32((uint32_t)k_uptime_ticks()) % 8192));
				if (msg_delay > 0) {
					k_sleep(K_MSEC(msg_delay));
				}
			}
		
		}
		if(uart_tx(serial_dev_data->uart_dev, 
				msg->data, msg->len, SYS_FOREVER_MS)) {
			LOG_WRN("Failed to send uart midi");
		}
	}
}

#define DEFINE_MIDI_SERIAL_IN_DEV_DATA(dev)										\
	static struct midi_serial_in_dev_data midi_serial_in_dev_data_##dev; 			

#define DEFINE_MIDI_SERIAL_OUT_DEV_DATA(dev)									\
	static struct midi_serial_out_dev_data midi_serial_out_dev_data_##dev = {	\
		COND_CODE_1(DT_PROP(SERIAL_OUT_DEV_N_ID(dev), handle_timestamps),		\
		(.timestamp_setting = MIDI_TIMESTAMP_ON,), 					\
		(.timestamp_setting = MIDI_TIMESTAMP_OFF,))																		\
	};																			\

#define DEFINE_MIDI_SERIAL_DEV_DATA(dev)				\
	COND_NODE_HAS_COMPAT_CHILD(MIDI_SERIAL_DEV_N_ID(dev), 		\
		COMPAT_MIDI_SERIAL_IN_DEVICE, 					\
		(DEFINE_MIDI_SERIAL_IN_DEV_DATA(dev)), ()) 		\
	COND_NODE_HAS_COMPAT_CHILD(MIDI_SERIAL_DEV_N_ID(dev), 		\
		COMPAT_MIDI_SERIAL_OUT_DEVICE, 					\
		(DEFINE_MIDI_SERIAL_OUT_DEV_DATA(dev)), ())		\
	static struct midi_serial_dev_data midi_serial_dev_data_##dev = {	\
		.uart_dev = DEVICE_DT_GET(SERIAL_DEV_N_ID(dev)),				\
		COND_NODE_HAS_COMPAT_CHILD(MIDI_SERIAL_DEV_N_ID(dev), 					\
			COMPAT_MIDI_SERIAL_IN_DEVICE, 								\
			(.in = &midi_serial_in_dev_data_##dev, ),				\
			(.in = NULL,)) 												\
		COND_NODE_HAS_COMPAT_CHILD(MIDI_SERIAL_DEV_N_ID(dev), 					\
			COMPAT_MIDI_SERIAL_OUT_DEVICE, 								\
			(.out = &midi_serial_out_dev_data_##dev,), 				\
			(.out = NULL,))												\
	};

#define DEFINE_MIDI_IN_DEVICE(dev)					  			\
	static struct midi_api midi_serial_in_api_##dev = {			\
		.midi_callback_set = midi_serial_in_port_callback_set,	\
	};															\
	DEVICE_DT_DEFINE(SERIAL_IN_DEV_N_ID(dev),			  			\
			    &midi_serial_in_device_init,			  			\
			    NULL,					  						\
			    &midi_serial_dev_data_##dev,			  		\
			    NULL, APPLICATION,	  							\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  		\
			    &midi_serial_in_api_##dev);

#define DEFINE_MIDI_OUT_DEVICE(dev)					  			\
	static struct midi_api midi_serial_out_api_##dev = {			\
		.midi_transfer = send_to_serial_port,					\
		.midi_callback_set = midi_serial_out_port_callback_set,	\
	};															\
	DEVICE_DT_DEFINE(SERIAL_OUT_DEV_N_ID(dev),			  			\
			    &midi_serial_out_device_init,			  			\
			    NULL,					  						\
			    &midi_serial_dev_data_##dev,			  	\
			    NULL, APPLICATION,	  							\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  		\
			    &midi_serial_out_api_##dev);

#define DEFINE_MIDI_IN_THREADS(dev)											\
	K_THREAD_DEFINE(midi_serial_rx_thread_##dev, 1024, 						\
		midi_rx_thread, &midi_serial_dev_data_##dev, NULL, NULL, 7, 0, 0);	\

#define DEFINE_MIDI_OUT_THREADS(dev)										\
	K_THREAD_DEFINE(midi_serial_tx_thread_##dev, 1024, 						\
		midi_tx_thread, &midi_serial_dev_data_##dev, NULL, NULL, 7, 0, 0);

#define MIDI_SERIAL_IN_DEVICE(dev)   \
	DEFINE_MIDI_IN_DEVICE(dev)   			\
	DEFINE_MIDI_IN_THREADS(dev)

#define MIDI_SERIAL_OUT_DEVICE(dev)  	\
	DEFINE_MIDI_OUT_DEVICE(dev)   				\
	DEFINE_MIDI_OUT_THREADS(dev)

#define MIDI_SERIAL_DEVICE(dev, _) \
	COND_CODE_1(UTIL_AND(DT_NODE_HAS_COMPAT(SERIAL_DEV_N_ID(dev), \
		nordic_nrf_uarte), DT_NODE_HAS_STATUS(SERIAL_DEV_N_ID(dev), okay)),( \
	DEFINE_MIDI_SERIAL_DEV_DATA(dev)\
	COND_NODE_HAS_COMPAT_CHILD(MIDI_SERIAL_DEV_N_ID(dev), \
		COMPAT_MIDI_SERIAL_IN_DEVICE, \
		(MIDI_SERIAL_IN_DEVICE(dev)), ()) \
	COND_NODE_HAS_COMPAT_CHILD(MIDI_SERIAL_DEV_N_ID(dev), \
		COMPAT_MIDI_SERIAL_OUT_DEVICE, \
		(MIDI_SERIAL_OUT_DEVICE(dev)), ())), ())

LISTIFY(MIDI_DEVICE_COUNT, MIDI_SERIAL_DEVICE, ());



