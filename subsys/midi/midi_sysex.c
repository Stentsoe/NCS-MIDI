/**
 * @file
 * @brief MIDI sysex
 *
 * Driver for MIDI sysex
 */
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include "midi/midi_sysex.h"
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_sysex
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

midi_sysex_universal_msg_header_t *midi_sysex_universal_msg_header_parse(midi_msg_t *msg)
{
    midi_sysex_universal_msg_header_t *sysex_header;
    
    if (!msg) {
        LOG_ERR("invalid message");
        return NULL;
    }

    if (msg->len < sizeof(midi_sysex_universal_msg_header_t)) {
        LOG_ERR("invalid message length");
        return NULL;
    }

    sysex_header = (midi_sysex_universal_msg_header_t *)msg->data;
    
    return sysex_header;
}

void midi_sysex_universal_msg_header_create(midi_sysex_universal_msg_header_t *header, 
            uint8_t device_id, uint8_t sub_id_1, uint8_t sub_id_2)
{
    header->sysex_start = MIDI_SYSEX_START;
    header->manufacturer_id = MIDI_SYSEX_UNIVERSAL_NON_REAL_TIME;
    header->device_id = device_id;
    header->sub_id_1 = sub_id_1;
    header->sub_id_2 = sub_id_2;
}
