#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>

#include "test_gpio_out.h"

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME gpio
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#if !DT_NODE_EXISTS(DT_NODELABEL(test_gpio_out_0))
#error "Overlay for test_gpio_0 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(test_gpio_out_1))
#error "Overlay for test_gpio_1 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(test_gpio_out_2))
#error "Overlay for test_gpio_2 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(test_gpio_out_3))
#error "Overlay for test_gpio_3 node not properly defined."
#endif

static const struct gpio_dt_spec test_gpio_out_0 =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(test_gpio_out_0), gpios, {0});

static const struct gpio_dt_spec test_gpio_out_1 =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(test_gpio_out_1), gpios, {0});

static const struct gpio_dt_spec test_gpio_out_2 =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(test_gpio_out_2), gpios, {0});

static const struct gpio_dt_spec test_gpio_out_3 =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(test_gpio_out_3), gpios, {0});

static const struct gpio_dt_spec gpios[4] =  {
    test_gpio_out_0, 
    test_gpio_out_1, 
    test_gpio_out_2, 
    test_gpio_out_3};

int toggle_test_gpio(uint8_t gpio)
{
    	int err;
        err = gpio_pin_toggle(gpios[gpio].port, gpios[gpio].pin);
		if(err) {
			LOG_INF("failed to toggle gpio %d", gpio);
		}
        return err;
}

void test_gpio_out_init(void)
{
	int err;
	LOG_INF("Test sample running");

	if (!gpio_is_ready_dt(&test_gpio_out_0)) {
		LOG_INF("The test_gpio_out_0 GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&test_gpio_out_1)) {
		LOG_INF("The test_gpio_out_1 GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&test_gpio_out_2)) {
		LOG_INF("The test_gpio_out_2 GPIO port is not ready.\n");
		return;
	}

	if (!gpio_is_ready_dt(&test_gpio_out_3)) {
		LOG_INF("The test_gpio_out_3 GPIO port is not ready.\n");
		return;
	}

	err = gpio_pin_configure_dt(&test_gpio_out_0, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_INF("Configuring GPIO failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&test_gpio_out_1, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_INF("Configuring GPIO failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&test_gpio_out_2, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_INF("Configuring GPIO failed: %d\n", err);
		return;
	}

	err = gpio_pin_configure_dt(&test_gpio_out_3, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_INF("Configuring GPIO failed: %d\n", err);
		return;
	}
}
