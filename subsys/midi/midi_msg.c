/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MIDI device class driver
 *
 * Driver for USB MIDI device class driver
 */

#include "midi/midi.h"
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_msg
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

midi_msg_t  * __must_check midi_msg_alloc(midi_msg_t * msg, size_t size) 
{
	if (!msg) {
		msg = k_malloc(sizeof(*msg));
		if (!msg) {
			return NULL;
		}
		memset(msg, 0, sizeof(*msg));
	}

	if (size) {
		if (msg->data) {
			k_free(msg->data);
		}
		msg->data = k_malloc(size);
		msg->ref = 1;
	}
    
	return msg;
}

midi_msg_t  * __must_check midi_msg_init(struct net_buf *buf,
						uint8_t *data, uint8_t len, enum midi_format format,
						void *context, uint16_t timestamp, int64_t uptime, 
						uint8_t num, uint8_t ack_channel)
{

	midi_msg_t *msg = k_malloc(sizeof(midi_msg_t));
	if (!msg) {
		return NULL;
	}

	msg->data = data;
	msg->len = len;
	msg->ref = 1;
	msg->format = format;
	msg->context = context;
	msg->buf = net_buf_ref(buf);
	msg->timestamp = timestamp;
	msg->uptime = uptime;
	msg->num = num;
	msg->ack_channel = ack_channel;
    
	return msg;
}

midi_msg_t  * __must_check midi_msg_init_alloc(midi_msg_t * msg, uint8_t len, 
								enum midi_format format, void *context) 
{
	msg = midi_msg_alloc(msg, len);
	if (!msg)
	{
		LOG_INF("failed to alloc midi msg");
		return NULL;
	}
	
	msg->len = len;
	msg->format = format;
	msg->context = context;
    
	return msg;
}

midi_msg_t * __must_check midi_msg_ref(midi_msg_t *msg) 
{
	if (!msg) {
		return NULL;
	}

	msg->ref++;
	return msg;
}

void midi_msg_unref_alt(midi_msg_t *msg) 
{
	if (!msg) {
		return;
	}

    if (msg->ref == 0) {
		net_buf_unref(msg->buf);
		k_free(msg);
		return;
	}

	if (--msg->ref > 0) {
		return;
	}

	net_buf_unref(msg->buf);
	k_free(msg);
	return;
}

void midi_msg_unref(midi_msg_t *msg) 
{
	if (!msg) {
		return;
	}

    if (msg->ref == 0) {
        k_free(msg->data);
		k_free(msg);
		return;
	}

	if (--msg->ref > 0) {
		return;
	}

	k_free(msg->data);
	k_free(msg);
	return;
}