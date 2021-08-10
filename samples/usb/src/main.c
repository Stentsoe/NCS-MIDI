#include <zephyr/types.h>
#include <zephyr.h>
#include <device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <usb/usb_device.h>
#include <usb/class/usb_midi.h>
#include <drivers/midi.h>

#include <logging/log.h>

#define LOG_MODULE_NAME USB_midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

static void data_received(const struct device *dev,
			  struct net_buf *buffer,
			  size_t size)
{

	if (!buffer || !size) {
		/* This should never happen */
		return;
	}
	LOG_HEXDUMP_INF(buffer->data, size, "YO:");

	LOG_INF("Received %d data, buffer %p", size, buffer);

	net_buf_unref(buffer);
}
static struct midi_ops midi_in_ops = {
	.data_received_cb = data_received,
};
static const struct usb_midi_ops ops = {
};

void main(void)
{
	int blink_status = 0;
	int err;
	const struct device *usb_midi_dev;
	const struct device *midi_in_dev;
	const struct device *midi_out_dev;

	// // LOG_INF("Entered %s", __func__);
	// usb_midi_dev = device_get_binding("MIDI");

	// if (!usb_midi_dev) {
	// 	LOG_ERR("Can not get USB MIDI Device");
	// 	// return;
	// }

	// midi_in_dev = device_get_binding("MIDI_IN");
	// if (!midi_in_dev) {
	// 	LOG_ERR("Can not get MIDI IN PORT Device");
	// 	// return;
	// }

	// err = midi_callback_set(midi_in_dev, &midi_in_ops, NULL);
	// if (err != 0) {
	// 	LOG_ERR("Can not set MIDI IN callbacks");
	// }

	// midi_out_dev = device_get_binding("MIDI_OUT");
	// if (!midi_out_dev) {
	// 	LOG_ERR("Can not get MIDI OUT PORT Device");
	// 	// return;
	// }

	// // LOG_INF("Found USB MIDI Device");

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

