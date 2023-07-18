/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MIDI API
 *
 * MIDI API
 */

#ifndef ZEPHYR_INCLUDE_MIDI_BLUETOOTH_H_
#define ZEPHYR_INCLUDE_MIDI_BLUETOOTH_H_

#include <errno.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef int (*midi_bluetooth_connected)(struct bt_conn *conn, uint8_t conn_err);



int midi_bluetooth_register_connected_cb(midi_bluetooth_connected cb);
/**
 * @brief Advertise a bluetooth midi device.
 *
 * @param dev       MIDI device structure.
 *
 * @retval -ENOTSUP If not supported.
 * @retval 0	    If successful, negative errno code otherwise.
 */
int midi_bluetooth_advertise(const struct device *dev);

/**
 * @brief Scan for bluetooth midi devices.
 *
 * @param dev       MIDI device structure.
 *
 * @retval -ENOTSUP If not supported.
 * @retval 0	    If successful, negative errno code otherwise.
 */
int midi_bluetooth_scan(const struct device *dev);

/**
 * @brief Disconnect a bluetooth midi device.
 *
 * @param dev       MIDI device structure.
 *
 * @retval -ENOTSUP If not supported.
 * @retval 0	    If successful, negative errno code otherwise.
 */
int midi_bluetooth_disconnect(const struct device *dev);



void print_test();
#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_DRIVERS_UART_H_ */
