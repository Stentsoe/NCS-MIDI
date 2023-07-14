/**
 * @file midi_parser.h
 *
 * @defgroup midi_parser MIDI parser
 * @{
 * @brief Basic parser for serial MIDI streams.
 */
#ifndef MIDI_PARSER_H__
#define MIDI_PARSER_H__


#include <stdbool.h>
// #include <zephyr.h>
#include <zephyr/types.h>
#include <midi/midi.h>


#ifdef __cplusplus
extern "C" {
#endif

#define DEFINE_MIDI_PARSER_SERIAL(_name) \
	struct midi_serial_parser _name;

#define DEFINE_MIDI_PARSER_USB(_name) \
	struct midi_usb_parser _name;

/** @brief Struct holding the state of the MIDI parser instance. */
struct midi_serial_parser {
	uint8_t * data_start;
	midi_msg_t *msg;
	uint8_t running_status;
	bool third_byte_flag;
};

// struct midi_serial_multi_parser {
// 	struct midi_serial_byte_parser byte_parser;
// 	midi_msg_t *msg;
// 	uint8_t len;
// };

struct midi_usb_parser {
	midi_msg_t *msg;
};

// struct midi_parser {
// 	union {
// 		struct midi_serial_parser serial;
// 		struct midi_usb_parser usb;
// 	};
// };

/**
 * @brief Parse a MIDI stream of messages with single byte format
 *
 * This function parses a MIDI stream of single byte messages. Function should be called
 * per msg of the stream, and the @p parser state has to be maintained 
 * between each call. When the function returns true the parsed message is completed.
 *
 * @param msg        The message to be parsed. Needs to be in format @ref MIDI_FORMAT_1_0_SERIAL.
 *
 * @param parser     Pointer to the parser of the MIDI stream
 *
 * @retval pointer to the parsed message if message is complete.
 * @retval NULL if the message is not completed or format is wrong.
 *
 */
// midi_msg_t *midi_parse_serial_byte(midi_msg_t *msg,
// 			    struct midi_serial_parser *parser);

midi_msg_t *midi_parse_serial(midi_msg_t *msg, struct midi_serial_parser *parser);

midi_msg_t *midi_parse_usb(midi_msg_t *msg, struct midi_usb_parser *parser);

// midi_msg_t *midi_parse_msg(midi_msg_t *msg, struct midi_parser *parser);

/**
 * @brief Reset a parser to initial conditions
 */
void midi_serial_parser_reset(struct midi_serial_parser *parser);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MIDI_SERIAL_PARSER_H__ */
