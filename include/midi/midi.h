/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MIDI API
 *
 * MIDI API
 */

#ifndef ZEPHYR_INCLUDE_MIDI_H_
#define ZEPHYR_INCLUDE_MIDI_H_

#include <errno.h>
#include <stddef.h>
#include <kernel.h>

#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_TIME_13BIT(time) (uint16_t)((time)&8191)


enum midi_format {
	MIDI_FORMAT_1_0_PARSED,
	MIDI_FORMAT_1_0_SERIAL,
	MIDI_FORMAT_1_0_USB,
};

/** @brief Struct holding a MIDI message. */
typedef struct {
    /** reserved for FIFO use. */
	void *fifo_reserved;
	/** format of midi message */
	enum midi_format format;
    /** timestamp, 13 bits are used with ms resolution */
	uint16_t timestamp;
    /** MIDI data */
	uint8_t *data;
    /** length of MIDI data */
	uint8_t len;
	/** reference count */
	uint8_t ref;
} midi_msg_t;

/**
 * @brief Callback type used to inform the app that data were successfully
 *	  send/received.
 *
 * @param dev	 The device for which the callback was called.
 * @param buffer Pointer to the net_buf data chunk that was successfully
 *		 send/received. If the application uses data_written_cb and/or
 *		 data_received_cb callbacks it is responsible for freeing the
 *		 buffer by itself.
 * @param size	 Amount of data that were successfully send/received.
 */
typedef int (*midi_transfer)(const struct device *dev,
				    midi_msg_t *msg,
					void *user_data);

struct midi_api {
	midi_transfer midi_transfer;

	midi_transfer midi_transfer_done;

	int (*midi_callback_set)(const struct device *dev,
						midi_transfer cb,
						void *user_data);

};

/**
 * @brief Set event handler function.
 *
 * @param dev       MIDI device structure.
 * @param callback  Event handler.
 * @param user_data Data to pass to event handler function.
 *
 * @retval -ENOTSUP If not supported.
 * @retval 0	    If successful, negative errno code otherwise.
 */
static inline int midi_send(const struct device *dev,
				    midi_msg_t *msg)
{
	const struct midi_api *api =
			(const struct midi_api *)dev->api;
    if (api != NULL) {
        if (api->midi_transfer != NULL) {
            return api->midi_transfer(dev, msg, NULL);
        }
    }
	
	return -ENOTSUP;
}

/**
 * @brief Set event handler function.
 *
 * @param dev       MIDI device structure.
 * @param callback  Event handler.
 * @param user_data Data to pass to event handler function.
 *
 * @retval -ENOTSUP If not supported.
 * @retval 0	    If successful, negative errno code otherwise.
 */
static inline int midi_callback_set(const struct device *dev,
				    midi_transfer cb,
				    void *user_data)
{ 
	const struct midi_api *api =
			(const struct midi_api *)dev->api;
    if (api != NULL) {
        if (api->midi_callback_set != NULL) {
            return api->midi_callback_set(dev, cb, user_data);
        }
    }
	
	return -ENOTSUP;
}


midi_msg_t * __must_check midi_msg_alloc(midi_msg_t * msg, size_t size);

midi_msg_t * __must_check midi_msg_ref(midi_msg_t *msg);

void midi_msg_unref(midi_msg_t *msg);

// midi_msg_t * __must_check midi_msg_set(midi_msg_t *msg, uint8_t pos, uint8_t *data, size_t size)

void test_func(void);

#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_DRIVERS_UART_H_ */
