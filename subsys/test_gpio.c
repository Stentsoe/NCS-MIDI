/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MIDI device class driver
 *
 * Driver for USB MIDI device class driver
 */

#include "test_gpio.h"
#include <hal/nrf_gpio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME test_gpio
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

int test_gpio_toggle(uint8_t pin)
{
	int err;
	err = gpio_pin_toggle(gpio_dev, pin);
	if (err) {
		LOG_ERR("GPIO_0 toggle error: %d", err);
	}
	return err;
}

int test_gpio_init(uint8_t pin)
{
	int err;

	if (!device_is_ready(gpio_dev)) {
		LOG_ERR("GPIO_0 device not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure(gpio_dev, pin,
				 GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (err) {
		LOG_ERR("GPIO_0 config error: %d", err);
		return err;
	}

	return 0;
}

