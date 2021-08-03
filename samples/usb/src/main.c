#include <zephyr/types.h>
#include <zephyr.h>
#include <device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>

#include <usb/class/usb_midi.h>

#include <logging/log.h>

#define LOG_MODULE_NAME USB_midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

static const struct usb_midi_ops ops = {
};

void main(void)
{

	const struct device *midi_dev;
	int ret;

	// LOG_INF("Entered %s", __func__);
	midi_dev = device_get_binding("MIDI");

	if (!midi_dev) {
		LOG_ERR("Can not get USB MIDI Device");
		// return;
	}

	// LOG_INF("Found USB MIDI Device");

	usb_midi_register(midi_dev, &ops);
	
	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	LOG_INF("USB enabled");
	int blink_status = 0;
	int err;

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

