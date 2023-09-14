#include <zephyr/types.h>
// #include <zephyr.h>
#include <zephyr/device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <zephyr/usb/usb_device.h>
#include <usb/class/usb_midi.h>
#include <midi/midi.h>
#include <midi/midi_parser.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME USB_midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

const struct device *usb_midi_in_dev;
const struct device *usb_midi_out_dev;
const struct device *serial_midi_in_dev;
const struct device *serial_midi_out_dev;

DEFINE_MIDI_PARSER_SERIAL(serial_parser);
DEFINE_MIDI_PARSER_SERIAL(usb_serial_parser);

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
		midi_send(usb_midi_out_dev, parsed_msg);
	}
	return 0;
}

static int midi_usb_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{

	if (!msg) {
		/* This should never happen */
		return -EINVAL;
	}

	LOG_INF("MIDI sent! mhm");

	midi_msg_unref(msg);

	return 0;
}

static int midi_usb_received(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	midi_msg_t *parsed_msg = NULL;
	midi_msg_t * serial_msg;
	uint8_t pos = 1;

	if (!msg) {
		/* This should never happen */
		return -EINVAL;
	}
	LOG_HEXDUMP_INF(msg->data, msg->len, "main:");
	
	while(!parsed_msg) {
		serial_msg = midi_msg_alloc(NULL, 1);
        memcpy(serial_msg->data, (void*)(msg->data + pos), 1);
        serial_msg->format = MIDI_FORMAT_1_0_SERIAL;
        serial_msg->len = 1;
        parsed_msg = midi_parse_serial(serial_msg, &usb_serial_parser);
        midi_msg_unref(serial_msg);
		pos++;
        // if (parsed_msg) {
        //     return parsed_msg;
        // }
	}
	midi_msg_unref(msg);
	// midi_msg_unref(parsed_msg);
	LOG_HEXDUMP_INF(parsed_msg->data, parsed_msg->len, "parsed_msg:");
	midi_send(serial_midi_out_dev, parsed_msg);
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
	const struct device *usb_midi_dev;

	serial_midi_in_dev = get_port("SERIAL_MIDI_IN", 
		midi_serial_received, NULL);

	
	serial_midi_out_dev = get_port("SERIAL_MIDI_OUT", 
		midi_serial_sent, NULL);

	
	usb_midi_dev = device_get_binding("USB_MIDI");
	if (!usb_midi_dev) {
		LOG_ERR("Can not get USB MIDI Device");
		return;
	}

	usb_midi_in_dev = get_port("USB_MIDI_IN", 
		midi_usb_received, NULL);

	
	usb_midi_out_dev = get_port("USB_MIDI_OUT", 
		midi_usb_sent, NULL);


	// usb_midi_register(usb_midi_dev, &ops);
	
	err = usb_enable(NULL);
	if (err != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	// LOG_INF("USB enabled");

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

	test_func();

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}

