#include <zephyr/kernel.h>
#include "midi/midi_parser.h"
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi_parser
LOG_MODULE_REGISTER(LOG_MODULE_NAME);


static bool parse_serial_byte(uint16_t timestamp, uint8_t byte,
			    struct midi_serial_parser *parser)
{
	midi_msg_t *msg;
	uint8_t running_status;
	if (!parser->msg) {
		/** Byte is part of new message */
		parser->msg = midi_msg_alloc(parser->msg, 0);
		if (!parser->msg) {
			LOG_WRN("could not allocate midi buffer!");
			return false;
		}
		parser->msg->len = 0;
		parser->msg->timestamp = 0;
	}
	msg = parser->msg;
	running_status = parser->running_status;

	/** Setting timestamp */
	if (parser->msg->timestamp == 0) {
		msg->timestamp = timestamp;
	}
	if ((byte >> 7) == 1) {
		/** Current byte is statusbyte */
		parser->running_status = byte;
		parser->third_byte_flag = false;

		/** Message with only one byte */
		if ((byte >> 2) == 0b111101) {
			if (byte == 0xF7) {
				/** End of exclusive, not supported. Discarded for now.  */
				return false;
			}
			msg = midi_msg_alloc(msg, 1);
			if(!msg) {
				LOG_WRN("could not allocate midi buffer!");
				return false;
			}

			memcpy(msg->data, &byte, 1);
			msg->len = 1;

			return true;
		}
		return false;
	}
	if (parser->third_byte_flag == true) {
		/** Expected third, and last, byte of message */
		parser->third_byte_flag = false;
		memcpy(msg->data+2, &byte, 1);
		msg->len = 3;
		return true;
	}
	if (running_status == 0) {
		/** System Exclusive (SysEx) databytes, from 3rd byte until EoX, or
		 * orphaned databytes. */
		return false;
	}
	/** Channel Voice Messages */
	switch (running_status >> 4) {
	case 0x8:
	case 0x9:
	case 0xA:
	case 0xB:
	case 0xE:
		parser->third_byte_flag = true;
		msg = midi_msg_alloc(msg, 3);
		if(!msg) {
			LOG_WRN("could not allocate midi buffer!");
			return false;
		}
		memcpy(msg->data, &running_status, 1);
		memcpy(msg->data+1, &byte, 1);
		msg->len = 2;
		
		return false;
	case 0xC:
	case 0xD:
		msg = midi_msg_alloc(msg, 2);
		if(!msg) {
			LOG_WRN("could not allocate midi buffer!");
			return false;
		}

		memcpy(msg->data, &running_status, 1);
		
		memcpy(msg->data+1, &byte, 1);
		
		msg->len = 2;
		return true;
	}
	/** System Common Message */
	switch (running_status) {
	case 0xF2:
		parser->third_byte_flag = true;
		msg = midi_msg_alloc(msg, 3);
		if(!msg) {
			LOG_WRN("could not allocate midi buffer!");
			return false;
		}
		memcpy(msg->data, &running_status, 1);
		memcpy(msg->data+1, &byte, 1);
		msg->len = 2;
		parser->running_status = 0;
		return false;
	case 0xF1:
	case 0xF3:
		parser->third_byte_flag = true;
		msg = midi_msg_alloc(msg, 3);
		if(!msg) {
			LOG_WRN("could not allocate midi buffer!");
			return false;
		}
		memcpy(msg->data, &running_status, 1);
		memcpy(msg->data+1, &byte, 1);
		msg->len = 2;
		parser->running_status = 0;
		return true;
	case 0xF0:
		break;
	}
	parser->running_status = 0;
	return false;
}

midi_msg_t *parse_serial_rtm(uint16_t timestamp, uint8_t byte)
{
	midi_msg_t *msg;

	if (((byte >> 7) == 1) && ((byte >> 3) == 0b11111)) {
		/** System Real-Time Messages */
		msg = midi_msg_alloc(NULL, 1);
		if (!msg) {
			return NULL;
		}
		memcpy(msg->data, &byte, 1);
		msg->len = 1;
		msg->timestamp = timestamp;

		if (byte == 0xFF) {
			/** MIDI Reset message, reset device to initial conditions */
		}

		return msg;
	}

	return NULL;
}

midi_msg_t *midi_parse_serial_byte(midi_msg_t *msg,
			    struct midi_serial_parser *parser)
{
    midi_msg_t *ret = NULL;

    ret = parse_serial_rtm(msg->timestamp, *msg->data);
	if (!ret) {
		/** Received byte is System Common- or Channel Voice message. */
		if (parse_serial_byte(msg->timestamp, *msg->data, parser)) {
			ret = parser->msg;
			parser->msg = NULL;
		}
	} else if (parser->msg && (parser->msg->timestamp < msg->timestamp)) {
		/** RTM has interrupted a message */
		parser->msg->timestamp = msg->timestamp;
	}
	return ret;
}

midi_msg_t *midi_parse_serial(midi_msg_t *msg, struct midi_serial_parser *parser) 
{
	if(msg->format != MIDI_FORMAT_1_0_SERIAL) {
		LOG_WRN("Tried to parse midi format that wasnt serial format: %d", msg->format);
		return NULL;
	}

	midi_msg_t *ret = NULL;
	if (!parser->data_start) {
		parser->data_start = msg->data;
	}
	
	while (!ret) {
		ret = parse_serial_rtm(msg->timestamp, *msg->data);
		if (!ret) {
			/** Received byte is System Common- or Channel Voice message. */
			if (parse_serial_byte(msg->timestamp, *msg->data, parser)) {
				ret = parser->msg;
				parser->msg = NULL;
			}
		} else if (parser->msg && (parser->msg->timestamp < msg->timestamp)) {
			/** RTM has interrupted a message */
			parser->msg->timestamp = msg->timestamp;
		}
		
		msg->len--; 
		msg->data ++;

		if (msg->len == 0) {
			msg->data = parser->data_start;
			parser->data_start = NULL;
			break;
		}
	}
	return ret;
}

midi_msg_t *midi_parse_usb(midi_msg_t *msg, struct midi_usb_parser *parser) 
{
	// if(msg->format != MIDI_FORMAT_1_0_USB) {
	// 	LOG_WRN("Tried to parse midi format that wasnt serial format: %d", msg->format);
		return NULL;
	// }

	// midi_msg_t *ret = NULL;
	// if (!parser->data_start) {
	// 	parser->data_start = msg->data;
	// }
	
	// while (!ret) {
	// 	ret = parse_serial_rtm(msg->timestamp, *msg->data);
	// 	if (!ret) {
	// 		/** Received byte is System Common- or Channel Voice message. */
	// 		if (parse_serial_byte(msg->timestamp, *msg->data, parser)) {
	// 			ret = parser->msg;
	// 			parser->msg = NULL;
	// 		}
	// 	} else if (parser->msg && (parser->msg->timestamp < msg->timestamp)) {
	// 		/** RTM has interrupted a message */
	// 		parser->msg->timestamp = msg->timestamp;
	// 	}
		
	// 	msg->len--; 
	// 	msg->data ++;

	// 	if (msg->len == 0) {
	// 		msg->data = parser->data_start;
	// 		parser->data_start = NULL;
	// 		break;
	// 	}
	// }
	// return ret;
}

void midi_parser_reset(struct midi_serial_parser *parser)
{
    if (parser->msg != NULL) {
        k_free(parser->msg);
    }
    parser->running_status = 0;
    parser->third_byte_flag = false;
}