#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <midi/midi.h>
#include <midi/midi_bluetooth.h>
#include <midi/midi_parser.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

const struct device *serial_midi_in_dev;
const struct device *serial_midi_out_dev;
const struct device *bluetooth_midi_in_dev;
const struct device *bluetooth_midi_out_dev;

DEFINE_MIDI_PARSER_SERIAL(serial_parser);
DEFINE_MIDI_PARSER_BLUETOOTH(bluetooth_parser);

int midi_bluetooth_connected_cb(struct bt_conn *conn, uint8_t conn_err) 
{
	LOG_INF("MIDI BLUETOOTH connected!");
	
	dk_set_led_on(CON_STATUS_LED);

	return 0;
}

static int midi_bluetooth_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	LOG_INF("MIDI BLUETOOTH sent!");
	
	midi_msg_unref(msg);

	return 0;
}

static int midi_bluetooth_received(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	LOG_INF("MIDI BLUETOOTH received!");
	midi_msg_t *parsed_msg = midi_parse_serial(msg, &bluetooth_parser);
	midi_msg_unref(msg);

	if (parsed_msg) {
		LOG_HEXDUMP_INF(parsed_msg->data, parsed_msg->len, "parsed:");
		midi_send(serial_midi_out_dev, parsed_msg);
	}

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
		midi_send(bluetooth_midi_out_dev, parsed_msg);
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


	/*BLUETOOTH PORTS*/
	bluetooth_midi_in_dev = get_port("BLUETOOTH_MIDI_IN", 
			midi_bluetooth_received, NULL);

	bluetooth_midi_out_dev = get_port("BLUETOOTH_MIDI_OUT", 
			midi_bluetooth_sent, NULL);

	LOG_INF("ports gotten");
	midi_bluetooth_register_connected_cb(midi_bluetooth_connected_cb);

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}
	midi_bluetooth_scan(bluetooth_midi_in_dev);

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}