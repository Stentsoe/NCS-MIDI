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

midi_msg_t * __must_check midi_msg_ref(midi_msg_t *msg) 
{
	if (!msg) {
		return NULL;
	}

	msg->ref++;
	return msg;
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

// midi_msg_t * __must_check 
// 	midi_msg_set(midi_msg_t *msg, uint8_t pos, uint8_t *data, size_t size)
// {
// 	if (!msg || !msg.data) {
// 		return NULL;
// 	}

// 	memcpy(msg.data+pos, data, size);

// 	return msg.data;
// }
