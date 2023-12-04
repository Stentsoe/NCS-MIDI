

#ifndef ZEPHYR_INCLUDE_TEST_GPIO_IN
#define ZEPHYR_INCLUDE_TEST_GPIO_IN

#include <errno.h>
#include <stddef.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>

void test_gpio_in_init(void);

typedef void (*test_gpio_cb_t) (void);

void register_test_callback(test_gpio_cb_t cb, uint8_t gpio);
#endif /* ZEPHYR_INCLUDE_TEST_GPIO_IN */
