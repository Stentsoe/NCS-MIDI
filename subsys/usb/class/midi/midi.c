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

#include <kernel.h>
#include <usb/usb_common.h>
#include <usb/usb_device.h>
#include <usb_descriptor.h>
#include <usb/usbstruct.h>
#include <usb/class/usb_audio.h>
#include "usb_midi_internal.h"

#include <sys/byteorder.h>
#include <sys/util.h>
#include <net/buf.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);




void test_func(void) {
    // LOG_INF("%s", STRINGIFY(UTIL_LISTIFY(MIDI_DEVICE_COUNT, MIDI_DEVICE, MIDI)));

    LOG_INF("%s", TEST_STRING);
}



// UTIL_LISTIFY(MIDI_DEVICE_COUNT, MIDI_DEVICE, );