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

#ifndef ZEPHYR_INCLUDE_MIDI_ISO_H_
#define ZEPHYR_INCLUDE_MIDI_ISO_H_

#include <errno.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>

#include "midi/midi_ump.h"

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*midi_iso_synced)(struct bt_conn *conn, uint8_t conn_err);

int midi_iso_register_connected_cb(midi_iso_synced cb);
/**
 * @brief Advertise a bluetooth midi iso device.
 *
 * @param dev       MIDI device structure.
 *
 * @retval -ENOTSUP If not supported.
 * @retval 0	    If successful, negative errno code otherwise.
 */
int midi_iso_advertise(const struct device *dev);

/**
 * @brief Scan for bluetooth midi iso devices.
 *
 * @param dev       MIDI device structure.
 *
 * @retval -ENOTSUP If not supported.
 * @retval 0	    If successful, negative errno code otherwise.
 */
int midi_iso_scan(const struct device *dev);

/**
 * @brief Disconnect a bluetooth midi iso device.
 *
 * @param dev       MIDI device structure.
 *
 * @retval -ENOTSUP If not supported.
 * @retval 0	    If successful, negative errno code otherwise.
 */
int midi_iso_disconnect(const struct device *dev);

void midi_iso_set_function_block(midi_ump_function_block_t *function_block);

void midi_iso_ack_msg(uint8_t muid, uint8_t msg_num);

void print_test();
#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_DRIVERS_UART_H_ */
