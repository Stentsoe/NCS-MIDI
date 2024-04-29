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
#include <zephyr/kernel.h>

#include <zephyr/net/buf.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_TIME_13BIT(time) (uint16_t)((time)&8191)

#define MIDI_OP_NOTE_OFF 0x8
#define MIDI_OP_NOTE_ON 0x9
#define MIDI_OP_POLYPHONIC_AFTERTOUCH 0xA
#define MIDI_OP_CONTROL_CHANGE 0xB
#define MIDI_OP_PROGRAM_CHANGE 0xC
#define MIDI_OP_CHANNEL_PRESSURE 0xD
#define MIDI_OP_PITCH_BEND 0xE

#define MIDI_INT_TO_DATA_1_BYTE(_dst, _data) \
 	memset(_dst, ((_data)&127), 1)
	
#define MIDI_INT_TO_DATA_2_BYTE(_dst, _data) \
	memset(_dst, ((_data)&127), 1); \
	memset(_dst + 1, (((_data) >> 7) & 127), 1)

#define MIDI_INT_TO_DATA_3_BYTE(_dst, _data) \
	memset(_dst, ((_data)&127), 1); \
	memset(_dst + 1, (((_data) >> 7) & 127), 1); \
	memset(_dst + 2, (((_data) >> 14) & 127), 1)

#define MIDI_INT_TO_DATA_4_BYTE(_dst, _data) \
	memset(_dst, ((_data)&127), 1); \
	memset(_dst + 1, (((_data) >> 7) & 127), 1); \
	memset(_dst + 2, (((_data) >> 14) & 127), 1); \
	memset(_dst + 3, (((_data) >> 21) & 127), 1)

#define MIDI_PTR_TO_DATA_1_BYTE(_dst, _data) \
	MIDI_INT_TO_DATA_1_BYTE(_dst, *((uint8_t *)_data))

#define MIDI_PTR_TO_DATA_2_BYTE(_dst, _data) \
	MIDI_INT_TO_DATA_2_BYTE(_dst, *((uint8_t *)_data))

#define MIDI_PTR_TO_DATA_3_BYTE(_dst, _data) \
	MIDI_INT_TO_DATA_3_BYTE(_dst, *((uint8_t *)_data))

#define MIDI_PTR_TO_DATA_4_BYTE(_dst, _data) \
	MIDI_INT_TO_DATA_4_BYTE(_dst, (*((uint8_t *)_data)))

#define MIDI_DATA_1_BYTE_TO_INTEGER(_byte_1) \
	((_byte_1) & 127)

#define MIDI_DATA_2_BYTE_TO_INTEGER(_byte_1, _byte_2) \
	((_byte_1) & 127) | \
	((_byte_2) & 127) << 7

#define MIDI_DATA_3_BYTE_TO_INTEGER(_byte_1, _byte_2, _byte_3) \
	((_byte_1) & 127) | \
	((_byte_2) & 127) << 7 | \
	((_byte_3) & 127) << 14

#define MIDI_DATA_4_BYTE_TO_INTEGER(_byte_1, _byte_2, _byte_3, _byte_4) \
	((_byte_1) & 127) | \
	((_byte_2) & 127) << 7 | \
	((_byte_3) & 127) << 14 | \
	((_byte_4) & 127) << 21

#define MIDI_DATA_ARRAY_1_BYTE_TO_INTEGER(_array) \
	MIDI_DATA_1_BYTE_TO_INTEGER(_array[0])

#define MIDI_DATA_ARRAY_2_BYTE_TO_INTEGER(_array) \
	MIDI_DATA_2_BYTE_TO_INTEGER(_array[0], _array[1])

#define MIDI_DATA_ARRAY_3_BYTE_TO_INTEGER(_array) \
	MIDI_DATA_3_BYTE_TO_INTEGER(_array[0], _array[1], _array[2])

#define MIDI_DATA_ARRAY_4_BYTE_TO_INTEGER(_array) \
	MIDI_DATA_4_BYTE_TO_INTEGER(_array[0], _array[1], _array[2], _array[3])

enum midi_format {
	MIDI_FORMAT_1_0_PARSED,
	MIDI_FORMAT_1_0_SERIAL,
	MIDI_FORMAT_1_0_USB,
	MIDI_FORMAT_1_0_PARSED_DELTA_US,
	MIDI_FORMAT_2_0_UMP,
};

typedef struct {
	uint8_t channel:4;
	uint8_t opcode:4;
} __packed midi_status_t;

typedef struct {
	midi_status_t status;
	uint8_t data_1;
} __packed midi_event_1_byte_t;

typedef struct {
	midi_status_t status;
	uint8_t data_1;
	uint8_t data_2;
} __packed midi_event_2_byte_t;

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
	/*uptime. NOTE: THIS VARIABLE IS A TEMPORARY WORKAROUND*/
	int64_t uptime;
	uint8_t num;
	uint8_t ack_channel;
	
	void * context;

	struct net_buf_t *buf;
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

midi_msg_t  * __must_check midi_msg_init(struct net_buf *buf,
						uint8_t *data, uint8_t len, enum midi_format format,
						void *context, uint16_t timestamp, int64_t uptime, 
						uint8_t num, uint8_t ack_channel);

midi_msg_t  * __must_check midi_msg_init_alloc(midi_msg_t * msg, uint8_t size, 
								enum midi_format format, void *context);

midi_msg_t * __must_check midi_msg_ref(midi_msg_t *msg);

void midi_msg_unref(midi_msg_t *msg);
void midi_msg_unref_alt(midi_msg_t *msg);

// midi_msg_t * __must_check midi_msg_set(midi_msg_t *msg, uint8_t pos, uint8_t *data, size_t size)

void test_func(void);

#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_MIDI_H_ */
