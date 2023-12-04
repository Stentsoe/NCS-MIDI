

#ifndef ZEPHYR_INCLUDE_TEST_GPIO_OUT
#define ZEPHYR_INCLUDE_TEST_GPIO_OUT

#include <errno.h>
#include <stddef.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>

void test_gpio_out_init(void);

int toggle_test_gpio(uint8_t gpio);
#endif /* ZEPHYR_INCLUDE_TEST_GPIO_OUT */
