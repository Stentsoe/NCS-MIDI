

#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>

#include <test_gpio_in.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME t
LOG_MODULE_REGISTER(LOG_MODULE_NAME);


#if !DT_NODE_EXISTS(DT_NODELABEL(packet_lost_in))
#error "Overlay for packet_lost_in node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(message_rdy_in))
#error "Overlay for message_rdy_in node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(message_sent_in))
#error "Overlay for message_sent_in node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(send_message_out))
#error "Overlay for send_message_out node not properly defined."
#endif

static const struct gpio_dt_spec packet_lost_in =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(packet_lost_in), gpios, {0});
static struct gpio_callback packet_lost_in_cb_data;

static const struct gpio_dt_spec message_rdy_in =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(message_rdy_in), gpios, {0});
static struct gpio_callback message_rdy_in_cb_data;

static const struct gpio_dt_spec message_sent_in =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(message_sent_in), gpios, {0});
static struct gpio_callback message_sent_in_cb_data;

static const struct gpio_dt_spec send_message_out =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(send_message_out), gpios, {0});

void packet_lost_in_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	int64_t time = k_ticks_to_us_near64(k_uptime_ticks());
	LOG_RAW("\nL %lld", time);
}

void message_rdy_in_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	int64_t time = k_ticks_to_us_near64(k_uptime_ticks());
	LOG_RAW("\nR %lld", time);
}

void message_sent_in_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	int64_t time = k_ticks_to_us_near64(k_uptime_ticks());
	LOG_RAW("\nmessage sent in gpio toggled %lld", time);
}

void test_gpio_0_cb(void)
{
	int64_t time = k_ticks_to_us_near64(k_uptime_ticks());
	LOG_RAW("\n0 %lld", time);
}

void test_gpio_1_cb(void)
{
	int64_t time = k_ticks_to_us_near64(k_uptime_ticks());
	LOG_RAW("\n1 %lld", time);
}

void test_gpio_2_cb(void)
{
	int64_t time = k_ticks_to_us_near64(k_uptime_ticks());
	LOG_RAW("\n2 %lld", time);
}

void test_gpio_3_cb(void)
{
	int64_t time = k_ticks_to_us_near64(k_uptime_ticks());
	LOG_RAW("\n3 %lld", time);
}

void main(void)
{
	int err;
	LOG_INF("Test sample running");

	test_gpio_in_init();

	register_test_callback(test_gpio_0_cb, 0);
	register_test_callback(test_gpio_1_cb, 1);
	register_test_callback(test_gpio_2_cb, 2);
	register_test_callback(test_gpio_3_cb, 3);

	if (!gpio_is_ready_dt(&packet_lost_in)) {
		LOG_INF("The packet_lost_in pin GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&message_rdy_in)) {
		LOG_INF("The message_rdy_in pin GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&message_sent_in)) {
		LOG_INF("The message_sent_in pin GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&send_message_out)) {
		LOG_INF("The send_message_out pin GPIO port is not ready.\n");
		return;
	}

	err = gpio_pin_configure_dt(&packet_lost_in, GPIO_INPUT);
	if (err != 0) {
		LOG_INF("Configuring GPIO pin failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&message_rdy_in, GPIO_INPUT);
	if (err != 0) {
		LOG_INF("Configuring GPIO pin failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&message_sent_in, GPIO_INPUT);
	if (err != 0) {
		LOG_INF("Configuring GPIO pin failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&send_message_out, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_INF("Configuring GPIO pin failed: %d\n", err);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&packet_lost_in,
					      GPIO_INT_EDGE_BOTH);
	if (err != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			err, packet_lost_in.port->name, packet_lost_in.pin);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&message_rdy_in,
					      GPIO_INT_EDGE_BOTH);
	if (err != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			err, message_rdy_in.port->name, message_rdy_in.pin);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&message_sent_in,
					      GPIO_INT_EDGE_BOTH);
	if (err != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			err, message_sent_in.port->name, message_sent_in.pin);
		return;
	}

	gpio_init_callback(&packet_lost_in_cb_data, packet_lost_in_cb, 
		BIT(packet_lost_in.pin));

	gpio_init_callback(&message_rdy_in_cb_data, message_rdy_in_cb, 
		BIT(message_rdy_in.pin));

	gpio_init_callback(&message_sent_in_cb_data, message_sent_in_cb, 
		BIT(message_sent_in.pin));

	err = gpio_add_callback(packet_lost_in.port, &packet_lost_in_cb_data);
	if (err)
	{
		LOG_INF("gpio add callback failed");
	}
	
	err = gpio_add_callback(message_rdy_in.port, &message_rdy_in_cb_data);
	if (err)
	{
		LOG_INF("gpio add callback failed");
	}
	
	err = gpio_add_callback(message_sent_in.port, &message_sent_in_cb_data);
	if (err)
	{
		LOG_INF("gpio add callback failed");
	}
	int64_t time;
	for (;;)
	{

		k_sleep(K_USEC(105000));
		err = gpio_pin_toggle(send_message_out.port, send_message_out.pin);
		if(err) {
			LOG_INF("failed to toggle send message out pin");
		}
		time = k_ticks_to_us_near64(k_uptime_ticks());
		LOG_RAW("\nS %lld", time);


		// k_sleep(K_USEC(2000));
		// err = gpio_pin_toggle(send_message_out.port, send_message_out.pin);
		// if(err) {
		// 	LOG_INF("failed to toggle send message out pin");
		// }
		// time = k_ticks_to_us_near64(k_uptime_ticks());
		// LOG_RAW("\nS %lld", time);

	}
}
