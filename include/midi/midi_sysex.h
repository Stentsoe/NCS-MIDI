/*
*/


/**
* @file
* @brief MIDI_SYSEX
*
* MIDI_SYSEX
*/
#ifndef ZEPHYR_INCLUDE_MIDI_SYSEX_H_
#define ZEPHYR_INCLUDE_MIDI_SYSEX_H_

#include <errno.h>
#include <stddef.h>
#include <zephyr/kernel.h>

#include "midi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_SYSEX_START 0xF0
#define MIDI_SYSEX_END 0xF7

#define MIDI_SYSEX_UNIVERSAL_NON_REAL_TIME 0x7E
#define MIDI_SYSEX_UNIVERSAL_REAL_TIME 0x7F

enum midi_sysex_device_id {
    MIDI_SYSEX_DEVICE_ID_1_CHAN_1 = 0x00,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_2 = 0x01,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_3 = 0x02,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_4 = 0x03,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_5 = 0x04,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_6 = 0x05,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_7 = 0x06,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_8 = 0x07,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_9 = 0x08,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_10 = 0x09,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_11 = 0x0A,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_12 = 0x0B,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_13 = 0x0C,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_14 = 0x0D,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_15 = 0x0E,
    MIDI_SYSEX_DEVICE_ID_1_CHAN_16 = 0x0F,
    MIDI_SYSEX_DEVICE_ID_1_GROUP = 0x7E,
    MIDI_SYSEX_DEVICE_ID_1_FUNCTION_BLOCK = 0x7F,
};

#define MIDI_SYSEX_SUB_ID_1_MIDI_CI 0x0D


/** @brief Struct holding a MIDI-CI message. */
typedef struct {
    uint8_t sysex_start;
    uint8_t manufacturer_id;
} __packed midi_sysex_msg_header_t;

/** @brief Struct holding a MIDI-CI message. */
typedef struct {
    uint8_t sysex_start;
    uint8_t manufacturer_id;
    enum midi_sysex_device_id device_id;
    uint8_t sub_id_1;
    uint8_t sub_id_2;
} __packed midi_sysex_universal_msg_header_t;

/** @brief Struct holding a MIDI-CI message. */
typedef struct {
    uint8_t sysex_end;
} __packed midi_sysex_msg_footer_t;

midi_sysex_universal_msg_header_t *midi_sysex_universal_msg_header_parse(midi_msg_t *msg);

void midi_sysex_universal_msg_header_create(midi_sysex_universal_msg_header_t *header, 
            uint8_t device_id, uint8_t sub_id_1, uint8_t sub_id_2);
            
#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_MIDI_SYSEX_H_ */