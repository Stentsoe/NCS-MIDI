#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <midi/midi.h>
#include <midi/midi_iso.h>
#include <midi/midi_parser.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

const struct device *serial_midi_in_dev;
const struct device *serial_midi_out_dev;
const struct device *iso_midi_out_dev;

DEFINE_MIDI_PARSER_SERIAL(serial_parser);

static int midi_iso_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	LOG_INF("MIDI ISO sent!");
	
	midi_msg_unref(msg);

	return 0;
}

static int midi_serial_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	LOG_INF("MIDI SERIAL sent!");
	
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
		midi_send(iso_midi_out_dev, parsed_msg);
	}
	
	return 0;
}

const struct device * get_port(const char* name, 
			midi_transfer cb, void *user_data)
{
	int err;
	const struct device * dev;
	dev = device_get_binding(name);

	if (!dev) {
		LOG_ERR("Can not get device: %s", name);
		return ENODEV;
	}

	err = midi_callback_set(dev, cb, user_data);
	if (err != 0) {
		LOG_ERR("Can not set callbacks for device: %s", name);
	}

	return dev;
}

void main(void)
{
	int blink_status = 0;
	int err;
	LOG_INF("Running");

	/*SERIAL PORTS*/
	serial_midi_in_dev = get_port("SERIAL_MIDI_IN", 
			midi_serial_received, NULL);
	
	serial_midi_out_dev = get_port("SERIAL_MIDI_OUT", 
			midi_serial_sent, NULL);

	/*ISO PORTS*/
	iso_midi_out_dev = get_port("ISO_MIDI_BROADCASTER", 
			midi_iso_sent, NULL);

	LOG_INF("ports gotten");

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}
	// midi_iso_advertise(iso_midi_out_dev);

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}