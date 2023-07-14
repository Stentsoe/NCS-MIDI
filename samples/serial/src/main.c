#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <midi/midi.h>
#include <midi/midi_parser.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

const struct device *serial_midi_in_dev;
const struct device *serial_midi_out_dev;

DEFINE_MIDI_PARSER_SERIAL(serial_parser);

static int midi_serial_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	LOG_INF("MIDI sent!");
	
	midi_msg_unref(msg);

	return 0;
}

static int midi_serial_received(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	midi_msg_t *parsed_msg = midi_parse_serial(msg, &serial_parser);

	midi_msg_unref(msg);
	if (parsed_msg) {
		LOG_HEXDUMP_INF(parsed_msg->data, parsed_msg->len, "parsed:");
		midi_send(serial_midi_out_dev, parsed_msg);
	}
	
	return 0;
}

void main(void)
{
	int blink_status = 0;
	int err;

	serial_midi_in_dev = device_get_binding("SERIAL_MIDI_IN");

	if (!serial_midi_in_dev) {
		LOG_ERR("Can not get serial MIDI Device");
	}

	err = midi_callback_set(serial_midi_in_dev, midi_serial_received, NULL);
	if (err != 0) {
		LOG_ERR("Can not set MIDI IN callbacks");
	}

	serial_midi_out_dev = device_get_binding("SERIAL_MIDI_OUT");

	if (!serial_midi_out_dev) {
		LOG_ERR("Can not get serial MIDI Device");
		// return;
	}

	err = midi_callback_set(serial_midi_out_dev, midi_serial_sent, NULL);
	if (err != 0) {
		LOG_ERR("Can not set MIDI IN callbacks");
	}

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}