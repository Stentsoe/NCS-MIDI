#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>

#include <test_gpio_in.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME gpio
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#if !DT_NODE_EXISTS(DT_NODELABEL(test_gpio_in_0))
#error "Overlay for test_gpio_0 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(test_gpio_in_1))
#error "Overlay for test_gpio_1 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(test_gpio_in_2))
#error "Overlay for test_gpio_2 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(test_gpio_in_3))
#error "Overlay for test_gpio_3 node not properly defined."
#endif

static const struct gpio_dt_spec test_gpio_in_0 =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(test_gpio_in_0), gpios, {0});
static struct gpio_callback test_gpio_in_0_cb_data;

static const struct gpio_dt_spec test_gpio_in_1 =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(test_gpio_in_1), gpios, {0});
static struct gpio_callback test_gpio_in_1_cb_data;

static const struct gpio_dt_spec test_gpio_in_2 =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(test_gpio_in_2), gpios, {0});
static struct gpio_callback test_gpio_in_2_cb_data;

static const struct gpio_dt_spec test_gpio_in_3 =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(test_gpio_in_3), gpios, {0});
static struct gpio_callback test_gpio_in_3_cb_data;



test_gpio_cb_t callbacks[4];

void handle_cb(uint8_t gpio)
{
	if (callbacks[gpio]) {
		callbacks[gpio]();
	}
}

void test_gpio_in_0_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	handle_cb(0);
}

void test_gpio_in_1_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	handle_cb(1);
}

void test_gpio_in_2_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	handle_cb(2);
}

void test_gpio_in_3_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	handle_cb(3);
}

void register_test_callback(test_gpio_cb_t cb, uint8_t gpio)
{
	callbacks[gpio] = cb;
}

void test_gpio_in_init(void)
{
	int err;
	LOG_INF("Test sample running");

	if (!gpio_is_ready_dt(&test_gpio_in_0)) {
		LOG_INF("The test_gpio_in_0 GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&test_gpio_in_1)) {
		LOG_INF("The test_gpio_in_1 GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&test_gpio_in_2)) {
		LOG_INF("The test_gpio_in_2 GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&test_gpio_in_3)) {
		LOG_INF("The test_gpio_in_3 GPIO port is not ready.\n");
		return;
	}

	err = gpio_pin_configure_dt(&test_gpio_in_0, GPIO_INPUT);
	if (err != 0) {
		LOG_INF("Configuring GPIO failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&test_gpio_in_1, GPIO_INPUT);
	if (err != 0) {
		LOG_INF("Configuring GPIO failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&test_gpio_in_2, GPIO_INPUT);
	if (err != 0) {
		LOG_INF("Configuring GPIO failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&test_gpio_in_3, GPIO_INPUT);
	if (err != 0) {
		LOG_INF("Configuring GPIO failed: %d\n", err);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&test_gpio_in_0,
					      GPIO_INT_EDGE_BOTH);
	if (err != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			err, test_gpio_in_0.port->name, test_gpio_in_0.pin);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&test_gpio_in_1,
					      GPIO_INT_EDGE_BOTH);
	if (err != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			err, test_gpio_in_1.port->name, test_gpio_in_1.pin);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&test_gpio_in_2,
					      GPIO_INT_EDGE_BOTH);
	if (err != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			err, test_gpio_in_2.port->name, test_gpio_in_2.pin);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&test_gpio_in_3,
					      GPIO_INT_EDGE_BOTH);
	if (err != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			err, test_gpio_in_3.port->name, test_gpio_in_3.pin);
		return;
	}

	gpio_init_callback(&test_gpio_in_0_cb_data, test_gpio_in_0_cb, 
		BIT(test_gpio_in_0.pin));
	
	gpio_init_callback(&test_gpio_in_1_cb_data, test_gpio_in_1_cb, 
		BIT(test_gpio_in_1.pin));

	gpio_init_callback(&test_gpio_in_2_cb_data, test_gpio_in_2_cb, 
		BIT(test_gpio_in_2.pin));

	gpio_init_callback(&test_gpio_in_3_cb_data, test_gpio_in_3_cb, 
		BIT(test_gpio_in_3.pin));

	err = gpio_add_callback(test_gpio_in_0.port, &test_gpio_in_0_cb_data);
	if (err)
	{
		LOG_INF("gpio add callback failed");
	}	
	
	err = gpio_add_callback(test_gpio_in_1.port, &test_gpio_in_1_cb_data);
	if (err)
	{
		LOG_INF("gpio add callback failed");
	}	
	
	err = gpio_add_callback(test_gpio_in_2.port, &test_gpio_in_2_cb_data);
	if (err)
	{
		LOG_INF("gpio add callback failed");
	}	
	
	err = gpio_add_callback(test_gpio_in_3.port, &test_gpio_in_3_cb_data);
	if (err)
	{
		LOG_INF("gpio add callback failed");
	}
}
