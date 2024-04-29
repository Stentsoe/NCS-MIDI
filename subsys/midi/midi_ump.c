#include <zephyr/kernel.h>
#include "midi/midi_ump.h"
#include "midi/midi_ci.h"
#include <zephyr/logging/log.h>

#include <zephyr/random/rand32.h>

#define LOG_MODULE_NAME midi_ump
LOG_MODULE_REGISTER(LOG_MODULE_NAME);


uint32_t midi_ump_generate_muid(void) {
    return sys_rand32_get() & 0xFFFFFFF;
}

midi_ump_function_block_t * midi_ump_get_function_block_by_muid(midi_ump_endpoint_t *ump_endpoint, uint32_t muid) {
    midi_ump_function_block_t *function_block;
    SYS_SLIST_FOR_EACH_CONTAINER(&ump_endpoint->function_block_list, function_block, node) {
        if (function_block->muid == muid) {
            return function_block;
        }
    }
    return NULL;
}

midi_ump_function_block_t * midi_ump_get_remote_function_block_by_muid(midi_ump_function_block_t *function_block, uint32_t muid) {
    midi_ump_function_block_t *remote_function_block;
    SYS_SLIST_FOR_EACH_CONTAINER(&function_block->remote_function_block_list, remote_function_block, node) {
        if (remote_function_block->muid == muid) {
            return remote_function_block;
        }
    }
    return NULL;
}

void midi_ump_add_function_block(midi_ump_endpoint_t *ump_endpoint,
            midi_ump_function_block_t *function_block) 
{
    sys_slist_append(&ump_endpoint->function_block_list, &function_block->node);
    function_block->info.function_block_number = ump_endpoint->number_of_function_blocks;
    function_block->ump_endpoint = ump_endpoint;
    ump_endpoint->number_of_function_blocks++;
    function_block->muid = midi_ump_generate_muid();
}

int midi_ump_add_remote_function_block(midi_ump_function_block_t *function_block, 
                                        midi_ump_function_block_t *remote_function_block)
{
    if(midi_ump_get_remote_function_block_by_muid(function_block, remote_function_block->muid)) {
        LOG_INF("Remote function block already exists");
        return -EALREADY;
    }
    
    sys_slist_append(&function_block->remote_function_block_list, &remote_function_block->node);
    return 0;
}

void midi_ump_get_function_block(midi_msg_t *discovery_msg)
{
    midi_ci_discovery_msg_t *discovery_data = (midi_ci_discovery_msg_t *)discovery_msg->data;
    midi_ump_function_block_t * remote_function_block = k_malloc(sizeof(midi_ump_function_block_t));
    remote_function_block->name = NULL;
    remote_function_block->muid = MIDI_DATA_ARRAY_4_BYTE_TO_INTEGER(discovery_data->header.source_muid);
    remote_function_block->info.function_block_active = false;
    remote_function_block->info.function_block_number = 0;
    remote_function_block->info.direction = MIDI_UMP_FUNCTION_BLOCK_BIDIRECTIONAL;
    remote_function_block->info.midi_1_0 = MIDI_UMP_FUNCTION_BLOCK_MIDI_1_0_31250_BPS;
    remote_function_block->info.user_interface_hint = MIDI_UMP_FUNCTION_BLOCK_UNKNOWN;
    remote_function_block->info.first_group = 0;
    remote_function_block->info.number_of_groups = 0;
    remote_function_block->info.midi_ci_message_version = discovery_data->header.msg_version;
    remote_function_block->info.number_of_sysex_8_streams = 0;
}

int midi_ump_1_0_channel_voice_msg_create(midi_ump_1_0_channel_voice_msg_t *buf,
                                uint8_t group, uint8_t opcode, uint8_t channel, 
                                uint8_t data_1, uint8_t data_2)
{
    buf->header.message_type = MIDI_UMP_MSG_MIDI_1_0_CHANNEL_VOICE;
    buf->header.group = group;
    buf->channel_voice_msg.status.opcode = opcode;
    buf->channel_voice_msg.status.channel = channel;
    buf->channel_voice_msg.data_1 = data_1;
    buf->channel_voice_msg.data_2 = data_2;
    return 0;
}

midi_msg_t * midi_ump_1_0_channel_voice_msg_create_alloc(uint8_t group, uint8_t opcode, 
                                                        uint8_t channel, uint8_t data_1, 
                                                        uint8_t data_2, uint32_t *dest)
{
    int err;
    midi_msg_t *msg;

    msg = midi_msg_init_alloc(NULL, 4, MIDI_FORMAT_2_0_UMP, (void*)dest);
    if (!msg) {
        LOG_WRN("could not allocate midi 1.0 channel voice ump message!");
        return NULL;
    }

    err = midi_ump_1_0_channel_voice_msg_create((midi_ump_1_0_channel_voice_msg_t *)msg->data,
                                                group, opcode, channel, data_1, data_2);

    return msg;
}

uint8_t midi_ump_get_msg_group(midi_msg_t *msg)
{
    midi_ump_msg_header_t *header = (midi_ump_msg_header_t *)msg->data;
    return header->group;
}

uint8_t midi_ump_get_msg_type(midi_msg_t *msg)
{
    midi_ump_msg_header_t *header = (midi_ump_msg_header_t *)msg->data;
    return header->message_type;
}

midi_msg_t * midi_ump_32_msg_parse(uint8_t *data)
{
    midi_msg_t *msg;
    msg = midi_msg_init_alloc(NULL, 4, MIDI_FORMAT_2_0_UMP, NULL);
    if (!msg) {
        LOG_WRN("could not allocate midi 1.0 channel voice ump message!");
        return NULL;
    }
    memcpy(msg->data, data, 4);
    return msg;
}