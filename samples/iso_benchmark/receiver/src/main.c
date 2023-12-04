#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <zephyr/drivers/gpio.h>
#include <midi/midi.h>
#include <midi/midi_iso.h>
#include "test_gpio_out.h"

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

static K_FIFO_DEFINE(timestamp_queue);

const struct device *iso_midi_in_dev;

#if !DT_NODE_EXISTS(DT_NODELABEL(message_rdy_out))
#error "Overlay for message_rdy_out node not properly defined."
#endif

static const struct gpio_dt_spec message_rdy_out =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(message_rdy_out), gpios, {0});



int midi_iso_synced_cb(struct bt_conn *conn, uint8_t conn_err) 
{
	LOG_INF("MIDI ISO connected!");

	return 0;
}

static int midi_iso_received(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	// LOG_INF("MIDI ISO received!");
	// midi_msg_unref(msg);
	k_fifo_put(&timestamp_queue, msg);

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
	int err;
	midi_msg_t * msg;
	LOG_INF("Running");
	
	test_gpio_out_init();

	if (!gpio_is_ready_dt(&message_rdy_out)) {
		LOG_INF("The message_rdy_out pin GPIO port is not ready.\n");
		return;
	}

	err = gpio_pin_configure_dt(&message_rdy_out, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_INF("Configuring GPIO pin failed: %d\n", err);
		return;
	}

	/*ISO PORTS*/
	iso_midi_in_dev = get_port("ISO_MIDI_RECEIVER", 
			midi_iso_received, NULL);

	LOG_INF("ports gotten");

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}
	midi_iso_scan(iso_midi_in_dev);

	for (;;) {
		msg = k_fifo_get(&timestamp_queue, K_FOREVER);
		// if (msg->timestamp < 10)
		// {
		// 	msg->timestamp = 10;
		// }
		// LOG_INF("timestamp !!!!!!!!!!!!%d", msg->timestamp);
		k_sleep(K_USEC(msg->timestamp));
		err = gpio_pin_toggle(message_rdy_out.port, message_rdy_out.pin);
		if(err) {
			LOG_INF("failed to toggle send message out pin");
		}
		midi_msg_unref(msg);
	}
}
