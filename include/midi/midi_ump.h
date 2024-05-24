/*
*/


/**
* @file
* @brief MIDI_UMP
*
* MIDI_UMP
*/
#ifndef ZEPHYR_INCLUDE_MIDI_UMP_H_
#define ZEPHYR_INCLUDE_MIDI_UMP_H_

#include <errno.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include "midi/midi.h"

#include <zephyr/sys/util.h>

#include <zephyr/random/rand32.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_UMP_CI_CATEGORY_SUPPORTED_PROFILE_CONFIGURATION (1 << 2)
#define MIDI_UMP_CI_CATEGORY_SUPPORTED_PROPERTY_EXCHANGE     (1 << 3)
#define MIDI_UMP_CI_CATEGORY_SUPPORTED_PROCESS_INQUIRY       (1 << 4)

#define MIDI_UMP_REQUEST_ENDPOINT_INFO_NOTIFICATION         (1 << 0)
#define MIDI_UMP_REQUEST_DEVICE_IDENTITY_NOTIFICATION       (1 << 1)
#define MIDI_UMP_REQUEST_ENDPOINT_NAME_NOTIFICATION         (1 << 2)
#define MIDI_UMP_REQUEST_PRODUCT_INSTANCE_ID_NOTIFICATION   (1 << 3)
#define MIDI_UMP_REQUEST_STREAM_CONFIGURATION_NOTIFICATION  (1 << 4)

#define MIDI_UMP_REQUEST_FUNCTION_BLOCK_INFO_NOTIFICATION   (1 << 0)
#define MIDI_UMP_REQUEST_FUNCTION_BLOCK_NAME_NOTIFICATION   (1 << 1)

#define MIDI_UMP_MSG_UTILIYY 0x00
#define MIDI_UMP_MSG_SYSTEM_REAL_TIME 0x01
#define MIDI_UMP_MSG_SYSTEM_COMMON 0x01
#define MIDI_UMP_MSG_MIDI_1_0_CHANNEL_VOICE 0x02
#define MIDI_UMP_MSG_SYSEX 0x03
#define MIDI_UMP_MSG_DATA_64 0x03
#define MIDI_UMP_MSG_MIDI_2_0_CHANNEL_VOICE 0x04
#define MIDI_UMP_MSG_DATA_128 0x05
#define MIDI_UMP_MSG_FLEX_DATA 0x0D
#define MIDI_UMP_MSG_UMP_STREAM 0x0F


#define DEFINE_MIDI_UMP_ENDPOINT(_instance, _name, _product_instance_id, \
                _device_manufacturer_id, _device_family_id,\
                _device_family_model_number, _software_revision_level,  \
                _ump_version_major, _ump_version_minor, _protocol_support,\
                _midi_ci_message_version, _midi_ci_category_supported, \
                _jitter_reduction, _static_function_blocks, _max_sysex_size) \
    static midi_ump_endpoint_t _instance = { \
        .device_identifiers = { \
            .name = _name, \
            .product_instance_id = _product_instance_id, \
            .name_size = sizeof(_name), \
            .product_instance_id_size = sizeof(_product_instance_id), \
            .dev_manufacturer_id = _device_manufacturer_id, \
            .dev_family_id = _device_family_id, \
            .dev_family_model_number = _device_family_model_number, \
            .software_revision_level = _software_revision_level, \
        }, \
        .data_format_support = { \
            .ump_version_major = _ump_version_major, \
            .ump_version_minor = _ump_version_minor, \
            .protocol_support = _protocol_support, \
            .jitter_reduction = _jitter_reduction, \
        }, \
        .midi_ci_message_version = _midi_ci_message_version, \
        .midi_ci_category_supported = (_midi_ci_category_supported),\
        .static_function_blocks = _static_function_blocks, \
        .number_of_function_blocks = 0, \
        .max_sysex_size = _max_sysex_size, \
    }

#define DEFINE_MIDI_UMP_FUNCTION_BLOCK(_instance, _name, \
                _direction, _midi_1_0, _user_interface_hint, _first_group, \
                _number_of_groups, _midi_ci_message_version, \
                _number_of_sysex_8_streams) \
    static midi_ump_function_block_t _instance = { \
        .name = _name, \
        .info = { \
            .function_block_active = false, \
            .function_block_number = 0, \
            .direction = _direction, \
            .midi_1_0 = _midi_1_0, \
            .user_interface_hint = _user_interface_hint, \
            .first_group = _first_group, \
            .number_of_groups = _number_of_groups, \
            .midi_ci_message_version = _midi_ci_message_version, \
            .number_of_sysex_8_streams = _number_of_sysex_8_streams, \
        }, \
    }

#define DEFINE_MIDI_UMP_STREAM(_instance, _source, _dest) \
    static midi_stream_t _instance = { \
        .source = _source, \
        .dest = _dest, \
    }

enum midi_ump_format{
    MIDI_UMP_FORMAT_COMPLETE_MESSAGE,
    MIDI_UMP_FORMAT_START_MESSAGE,
    MIDI_UMP_FORMAT_CONTINUE_MESSAGE,
    MIDI_UMP_FORMAT_END_MESSAGE
};

enum midi_protocol {
    MIDI_PROTOCOL_1_0 = 0x01,
    MIDI_PROTOCOL_2_0 = 0x02,
};

enum midi_ump_function_block_direction {
    MIDI_UMP_FUNCTION_BLOCK_INPUT = 0x01,
    MIDI_UMP_FUNCTION_BLOCK_OUTPUT = 0x02,
    MIDI_UMP_FUNCTION_BLOCK_BIDIRECTIONAL = 0x03,
};

enum midi_ump_function_block_midi_1_0 {
    MIDI_UMP_FUNCTION_BLOCK_NOT_MIDI_1_0 = 0x00,
    MIDI_UMP_FUNCTION_BLOCK_MIDI_1_0_UNRESTRICTED_BANDWIDTH = 0x01,
    MIDI_UMP_FUNCTION_BLOCK_MIDI_1_0_31250_BPS = 0x02,
};

enum midi_ump_function_block_user_interface_hint {
    MIDI_UMP_FUNCTION_BLOCK_UNKNOWN = 0x00,
    MIDI_UMP_FUNCTION_BLOCK_RECEIVER = 0x01,
    MIDI_UMP_FUNCTION_BLOCK_SENDER = 0x02,
    MIDI_UMP_FUNCTION_BLOCK_RECEIVER_SENDER = 0x03,
};

typedef struct {
    const char *name;
    const char *product_instance_id;
    uint16_t name_size; 
    uint16_t product_instance_id_size;
    uint8_t dev_manufacturer_id[3];
    uint8_t dev_family_id[2];
    uint8_t dev_family_model_number[2];
    uint8_t software_revision_level[4];
} midi_ump_device_identifiers_t;

typedef struct {
    uint8_t ump_version_major;
    uint8_t ump_version_minor;
    uint8_t protocol_support;
    bool jitter_reduction;
} midi_ump_data_format_support_t;

typedef struct {
    midi_ump_device_identifiers_t device_identifiers;
    midi_ump_data_format_support_t data_format_support;
    uint8_t midi_ci_message_version;
    uint8_t midi_ci_category_supported;
    bool static_function_blocks;
    uint8_t number_of_function_blocks;
    uint32_t max_sysex_size;
    sys_slist_t function_block_list;
} midi_ump_endpoint_t;

typedef struct {
    bool function_block_active;
    uint8_t function_block_number;
    enum midi_ump_function_block_direction direction;
    enum midi_ump_function_block_midi_1_0 midi_1_0;
    enum midi_ump_function_block_user_interface_hint user_interface_hint;
    uint8_t first_group;
    uint8_t number_of_groups;
    uint8_t midi_ci_message_version;
    uint8_t number_of_sysex_8_streams;
} midi_ump_function_block_info_t;

typedef struct {
    sys_snode_t node;
    const char *name;
    uint32_t muid;
    midi_ump_function_block_info_t info;
    midi_ump_endpoint_t *ump_endpoint;
    sys_slist_t remote_function_block_list;
    sys_slist_t tx_msg_list;
} midi_ump_function_block_t;

typedef struct {
    uint8_t group:4;
    uint8_t message_type:4;
}__packed midi_ump_msg_header_t;

typedef struct {
    midi_ump_msg_header_t header;
    midi_event_2_byte_t channel_voice_msg;
} __packed midi_ump_1_0_channel_voice_msg_t;

typedef struct {
	midi_ump_function_block_t *source;
	midi_ump_function_block_t *dest;
} midi_stream_t;

typedef struct {
    sys_snode_t node;
    midi_msg_t *msg;
} midi_ump_tx_msg_t;

void midi_ump_add_function_block(midi_ump_endpoint_t *ump_endpoint,
                                midi_ump_function_block_t *function_block);

int midi_ump_add_remote_function_block(midi_ump_function_block_t *function_block,
                                        midi_ump_function_block_t *remote_function_block);
          
midi_ump_function_block_t * midi_ump_get_remote_function_block_by_muid(
                            midi_ump_function_block_t *function_block, uint32_t muid);

midi_msg_t * midi_ump_1_0_channel_voice_msg_create_alloc(uint8_t group, uint8_t opcode, uint8_t channel, 
                            uint8_t data_1, uint8_t data_2, uint32_t *dest);
                            
uint8_t midi_ump_get_msg_group(midi_msg_t *msg);

uint8_t midi_ump_get_msg_type(midi_msg_t *msg);

midi_msg_t * midi_ump_32_msg_parse(uint8_t *data);
                            
#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_MIDI_UMP_H_ */