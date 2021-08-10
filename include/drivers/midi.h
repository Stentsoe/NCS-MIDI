/*
 * Copyright (c) 2018-2019 Nordic Semiconductor ASA
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public APIs for MIDI ports
 */

#ifndef ZEPHYR_INCLUDE_MIDI_H_
#define ZEPHYR_INCLUDE_MIDI_H_

#include <errno.h>
#include <stddef.h>

#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif


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
// typedef void (*midi_data_received)(const struct device *dev,
// 					       struct net_buf *buffer,
// 					       size_t size);

typedef int (*midi_transfer)(const struct device *dev,
				    struct net_buf *buffer,
					size_t size);

struct midi_api {
	midi_transfer midi_transfer;

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
				    struct net_buf *buffer,
					size_t size)
{ 
	const struct midi_api *api =
			(const struct midi_api *)dev->api;
    if (api != NULL) {
        if (api->midi_transfer != NULL) {
            return api->midi_transfer(dev, buffer, size);
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



#ifdef __cplusplus
}
#endif

// #include <syscalls/midi.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_UART_H_ */
