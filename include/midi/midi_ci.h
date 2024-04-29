/*
 */

/**
 * @file
 * @brief MIDI API
 *
 * MIDI API
 */
#ifndef ZEPHYR_INCLUDE_MIDI_CI_H_
#define ZEPHYR_INCLUDE_MIDI_CI_H_

#include <errno.h>
#include <stddef.h>
#include <zephyr/kernel.h>

#include "midi.h"
#include "midi_ump.h"
#include "midi_sysex.h"

#ifdef __cplusplus
extern "C" {
#endif    

enum midi_ci_msg_type {
    MIDI_CI_MANAGEMENT_DISCOVERY = 0x70,
    MIDI_CI_MANAGEMENT_DISCOVERY_REPLY = 0x71,
    MIDI_CI_MANAGEMENT_ENDPOINT = 0x72,
};

enum midi_ci_msg_version {
    MIDI_CI_MSG_VERSION_1_1 = 0x01,
    MIDI_CI_MSG_VERSION_1_2 = 0x02,
};

enum midi_ci_status {
    MIDI_CI_STATUS_PRODUCT_INSTANCE_ID = 0x00,
};

/** @brief Struct holding a MIDI-CI message. */
typedef struct {
    midi_sysex_universal_msg_header_t universal_sysex_header;
    uint8_t msg_version;
    uint8_t source_muid[4];
    uint8_t dest_muid[4];
} __packed midi_ci_msg_header_t;

/** @brief Struct holding a MIDI-CI message. */
typedef struct {
    midi_sysex_msg_footer_t sysex_footer;
} __packed midi_ci_msg_footer_t;

/** @brief Struct holding a MIDI-CI message. */
typedef struct {
    uint8_t reserved_2:2;
    uint8_t process_inquiry:1;
    uint8_t property_exchange:1;
    uint8_t profile_configuration:1;
    uint8_t protocol_negotiation:1;
    uint8_t reserved:2;
} __packed midi_ci_category_supported_t;

/** @brief Struct holding MIDI-CI discovery message data */
typedef struct {
    midi_ci_msg_header_t header;
    uint8_t manufacturer_sysex_id[3];
    uint8_t dev_family[2];
    uint8_t dev_family_model_number[2];
    uint8_t software_revision[4];
    midi_ci_category_supported_t ci_category_supported[1];
    uint8_t max_sysex_size[4];
    uint8_t initiator_output_path_id[1];
    midi_ci_msg_footer_t footer;
} __packed midi_ci_discovery_msg_t;

/** @brief Struct holding MIDI-CI reply to discovery message data */
typedef struct {
    midi_ci_msg_header_t header;
    uint8_t manufacturer_sysex_id[3];
    uint8_t dev_family[2];
    uint8_t dev_family_model_number[2];
    uint8_t software_revision[4];
    midi_ci_category_supported_t ci_category_supported[1];
    uint8_t max_sysex_size[4];
    uint8_t initiator_output_path_id[1];
    uint8_t function_block[1];
    midi_ci_msg_footer_t footer;
} midi_ci_discovery_reply_msg_t;

/** @brief Struct holding MIDI-CI reply to discovery message data */
typedef struct {
    midi_ci_msg_header_t header;
    enum midi_ci_status status;
    midi_ci_msg_footer_t footer;
} midi_ci_endpoint_msg_t;

/** @brief Struct holding MIDI-CI reply to discovery message data */
typedef struct {
    midi_ci_msg_header_t header;
    enum midi_ci_status status;
    uint8_t length[2];
    void *data;
    midi_ci_msg_footer_t footer;
} midi_ci_endpoint_reply_msg_t;

midi_ci_msg_header_t *midi_ci_parse_msg_header(midi_msg_t *msg);

int midi_ci_discovery_msg_create(midi_ci_discovery_msg_t *buf, midi_ump_function_block_t *function_block);

midi_msg_t *midi_ci_discovery_msg_create_alloc(midi_ump_function_block_t *function_block);

midi_msg_t *midi_ci_discovery_reply_msg_create_alloc(midi_ump_function_block_t *function_block,
                    midi_ump_function_block_t *remote_function_block);

midi_msg_t *midi_ci_endpoint_msg_create(midi_ump_function_block_t *function_block,
                    midi_ump_function_block_t *remote_function_block);

midi_msg_t *midi_ci_endpoint_reply_msg_create(midi_ump_function_block_t *function_block,
                    midi_ump_function_block_t *remote_function_block);

midi_ump_function_block_t *midi_ci_parse_function_block_from_msg(midi_msg_t *msg);

midi_ump_function_block_t *midi_ci_parse_function_block_from_msg(midi_msg_t *msg);

midi_ci_discovery_msg_t *midi_ci_discovery_msg_parse(void *buf);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_UART_H_ */
