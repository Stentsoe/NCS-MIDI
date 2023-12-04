#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <zephyr/drivers/gpio.h>
#include <midi/midi.h>
#include <midi/midi_iso.h>
#include <test_gpio_out.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

#if !DT_NODE_EXISTS(DT_NODELABEL(send_message_in))
#error "Overlay for send_message_in node not properly defined."
#endif

static K_SEM_DEFINE(sem_send_msg, 0, 2);

static const struct gpio_dt_spec send_message_in =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(send_message_in), gpios, {0});
static struct gpio_callback send_message_in_cb_data;

const struct device *iso_midi_out_dev;

// midi_msg_t *msg;
uint8_t data[] = {0x90, 64, 85};

static int midi_iso_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	// LOG_INF("MIDI ISO sent!");
	
	midi_msg_unref(msg);

	return 0;
}

void send_message_in_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	// int64_t time = k_ticks_to_us_near64(k_uptime_ticks());
	LOG_INF("Send message");
	// int key = irq_lock();
	// k_sem_give(&sem_send_msg);
	midi_msg_t *msg = NULL;
	msg = midi_msg_alloc(msg, 3);
	msg->format = MIDI_FORMAT_1_0_PARSED;
	msg->len = 3;
	memcpy(msg->data, data, 3);
	midi_send(iso_midi_out_dev, msg);
	// irq_unlock(key);
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

	if (!gpio_is_ready_dt(&send_message_in)) {
		LOG_INF("The send_message_in pin GPIO port is not ready.\n");
		return;
	}

	err = gpio_pin_configure_dt(&send_message_in, GPIO_INPUT);
	if (err != 0) {
		LOG_INF("Configuring GPIO pin failed: %d\n", err);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&send_message_in,
					      GPIO_INT_EDGE_BOTH);
	if (err != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			err, send_message_in.port->name, send_message_in.pin);
		return;
	}

	gpio_init_callback(&send_message_in_cb_data, send_message_in_cb, 
		BIT(send_message_in.pin));
	err = gpio_add_callback(send_message_in.port, &send_message_in_cb_data);
	if (err)
	{
		LOG_INF("gpio add callback failed");
	}

	test_gpio_out_init();

	/*ISO PORTS*/
	iso_midi_out_dev = get_port("ISO_MIDI_BROADCASTER", 
			midi_iso_sent, NULL);

	LOG_INF("ports gotten");

	
	

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

	for (;;) {
		// err = k_sem_take(&sem_send_msg, K_FOREVER);
		// midi_send(iso_midi_out_dev, msg);
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));

	}
}