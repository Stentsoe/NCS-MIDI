/**
 * @file
 * @brief MIDI ci
 *
 * Driver for MIDI ci
 */
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/device.h>
#include <zephyr/sys/util.h>


#include "midi/midi_ci.h"
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_ci
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

midi_ci_msg_header_t *midi_ci_parse_msg_header(midi_msg_t *msg)
{
    if (!msg) {
        LOG_ERR("invalid message");
        return NULL;
    }

    if (msg->len < sizeof(midi_ci_msg_header_t)) {
        LOG_ERR("invalid message length");
        return NULL;
    }

    midi_ci_msg_header_t *header = (midi_ci_msg_header_t *)msg->data;
    return header;
}

void midi_ci_msg_header_create(midi_ci_msg_header_t *header, uint8_t sub_id_2, uint32_t source_muid, uint32_t dest_muid)
{
    midi_sysex_universal_msg_header_create((midi_sysex_universal_msg_header_t *)header, 
            MIDI_SYSEX_DEVICE_ID_1_FUNCTION_BLOCK, MIDI_SYSEX_SUB_ID_1_MIDI_CI, sub_id_2);
            
    header->msg_version = MIDI_CI_MSG_VERSION_1_2;
    MIDI_INT_TO_DATA_4_BYTE(header->source_muid, source_muid);
    MIDI_INT_TO_DATA_4_BYTE(header->dest_muid, dest_muid);
}

int midi_ci_discovery_msg_create(midi_ci_discovery_msg_t *buf, midi_ump_function_block_t *function_block)
{
    if (!buf) {
        LOG_ERR("invalid buffer");
        return -EINVAL;
    }

    midi_ump_endpoint_t *ep = function_block->ump_endpoint;
    midi_ump_device_identifiers_t *dev_id = &ep->device_identifiers;

    midi_ci_msg_header_create((midi_ci_msg_header_t *)buf,
            MIDI_CI_MANAGEMENT_DISCOVERY, function_block->muid, 
            (MIDI_DATA_4_BYTE_TO_INTEGER(0x7F, 0x7F, 0x7F, 0x7F)));
    
    MIDI_PTR_TO_DATA_3_BYTE(buf->manufacturer_sysex_id, dev_id->dev_manufacturer_id);
    MIDI_PTR_TO_DATA_2_BYTE(buf->dev_family, dev_id->dev_family_id);
    MIDI_PTR_TO_DATA_2_BYTE(buf->dev_family_model_number, dev_id->dev_family_model_number);
    MIDI_PTR_TO_DATA_4_BYTE(buf->software_revision, dev_id->software_revision_level);
    MIDI_INT_TO_DATA_1_BYTE(buf->ci_category_supported, ep->midi_ci_category_supported);
    MIDI_PTR_TO_DATA_4_BYTE(buf->max_sysex_size, ep->max_sysex_size);

    buf->initiator_output_path_id[0] = 0;
    buf->footer.sysex_footer.sysex_end = MIDI_SYSEX_END;

    return 0;
}

int midi_ci_discovery_reply_msg_create(midi_ci_discovery_reply_msg_t *buf, 
                                    midi_ump_function_block_t *function_block, 
                                    midi_ump_function_block_t *remote_function_block)
{
    if (!buf) {
        LOG_ERR("invalid buffer");
        return -EINVAL;
    }

    midi_ump_endpoint_t *ep = function_block->ump_endpoint;
    midi_ump_device_identifiers_t *dev_id = &ep->device_identifiers;

    midi_ci_msg_header_create((midi_ci_msg_header_t *)buf, 
            MIDI_CI_MANAGEMENT_DISCOVERY_REPLY, function_block->muid, remote_function_block->muid);
    
    MIDI_PTR_TO_DATA_3_BYTE(buf->manufacturer_sysex_id, dev_id->dev_manufacturer_id);
    MIDI_PTR_TO_DATA_2_BYTE(buf->dev_family, dev_id->dev_family_id);
    MIDI_PTR_TO_DATA_2_BYTE(buf->dev_family_model_number, dev_id->dev_family_model_number);
    MIDI_PTR_TO_DATA_4_BYTE(buf->software_revision, dev_id->software_revision_level);
    MIDI_INT_TO_DATA_1_BYTE(buf->ci_category_supported, ep->midi_ci_category_supported);
    MIDI_PTR_TO_DATA_4_BYTE(buf->max_sysex_size, ep->max_sysex_size);
    MIDI_INT_TO_DATA_1_BYTE(buf->function_block, function_block->info.function_block_number);

    buf->initiator_output_path_id[0] = 0;
    buf->footer.sysex_footer.sysex_end = MIDI_SYSEX_END;


    return 0;
}

midi_msg_t *midi_ci_discovery_msg_create_alloc(midi_ump_function_block_t *function_block)
{
    int err;
    
    midi_msg_t *msg = midi_msg_init_alloc(NULL, sizeof(midi_ci_discovery_msg_t), MIDI_FORMAT_1_0_PARSED, NULL);
    if (!msg) {
        LOG_WRN("could not allocate midi ci discovery message!");
        return NULL;
    }
    
    err = midi_ci_discovery_msg_create((midi_ci_discovery_msg_t *)msg->data, function_block);
    if (err)
    {
        LOG_WRN("could not create midi ci discovery message!");
        return NULL;
    }

    return msg;
}

midi_msg_t *midi_ci_discovery_reply_msg_create_alloc(midi_ump_function_block_t *function_block, 
            midi_ump_function_block_t *remote_function_block)
{
    int err;
    
    midi_msg_t *msg = midi_msg_init_alloc(NULL, sizeof(midi_ci_discovery_reply_msg_t), 
            MIDI_FORMAT_1_0_PARSED, NULL);
    if (!msg) {
        LOG_WRN("could not allocate midi ci discovery message!");
        return NULL;
    }
    
    err = midi_ci_discovery_reply_msg_create((midi_ci_discovery_reply_msg_t *)msg->data, 
                                    function_block, remote_function_block);
    if (err) {
        LOG_WRN("could not create midi ci discovery reply message!");
        return NULL;
    }

    return msg;
}

midi_ump_function_block_t *midi_ci_parse_function_block_from_msg(midi_msg_t *msg)
{
    midi_sysex_universal_msg_header_t *sysex_header;
    sysex_header = midi_sysex_universal_msg_header_parse(msg);

    if (sysex_header->sub_id_1 != MIDI_SYSEX_SUB_ID_1_MIDI_CI) {
        return NULL;
    }
    
    midi_ump_function_block_t * function_block;

    switch (sysex_header->sub_id_2)
    {
    case MIDI_CI_MANAGEMENT_DISCOVERY:
        function_block = k_malloc(sizeof(midi_ump_function_block_t));
        midi_ci_discovery_msg_t *discovery_data = (midi_ci_discovery_msg_t *)msg->data;
        function_block->muid = MIDI_DATA_ARRAY_4_BYTE_TO_INTEGER(discovery_data->header.source_muid);
        function_block->info.midi_ci_message_version = discovery_data->header.msg_version;
        break;
    case MIDI_CI_MANAGEMENT_DISCOVERY_REPLY:
        function_block = k_malloc(sizeof(midi_ump_function_block_t));
        midi_ci_discovery_reply_msg_t *reply_data = (midi_ci_discovery_reply_msg_t *)msg->data;
        function_block->muid = MIDI_DATA_ARRAY_4_BYTE_TO_INTEGER(reply_data->header.source_muid);
        function_block->info.midi_ci_message_version = reply_data->header.msg_version;
        break;
    
    default:
        return NULL;
        break;
    }

    function_block->name = NULL;
    function_block->info.function_block_active = false;
    function_block->info.function_block_number = 0;
    function_block->info.direction = MIDI_UMP_FUNCTION_BLOCK_BIDIRECTIONAL;
    function_block->info.midi_1_0 = MIDI_UMP_FUNCTION_BLOCK_MIDI_1_0_31250_BPS;
    function_block->info.user_interface_hint = MIDI_UMP_FUNCTION_BLOCK_UNKNOWN;
    function_block->info.first_group = 0;
    function_block->info.number_of_groups = 0;
    function_block->info.number_of_sysex_8_streams = 0;

    return function_block;
}

