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

/* Device data structure */
struct usb_midi_dev_data {
	const struct usb_midi_ops *ops;

	const struct cs_ac_if_descriptor *desc_hdr;

	struct usb_dev_data common;
};

#define DEFINE_MIDI_DEV_DATA(dev)	\
	static struct usb_midi_dev_data midi_dev_data_##dev;


void test_func(void) {
    // LOG_INF("%s", STRINGIFY(UTIL_LISTIFY(MIDI_DEVICE_COUNT, MIDI_DEVICE, MIDI)));

    LOG_INF("%s", TEST_STRING);
}


static void midi_interface_config(struct usb_desc_header *head,
				   uint8_t bInterfaceNumber)
{
	
}

static void midi_cb_usb_status(struct usb_cfg_data *cfg,
			 enum usb_dc_status_code cb_status,
			 const uint8_t *param)
{
	
}


/**
 * @brief Custom callback for USB Device requests.
 *
 * This callback is called when set/get interface request is directed
 * to the device. This is Zephyr way to address those requests.
 * It's not possible to do that in the core stack as common USB device
 * stack does not know the amount of devices that has alternate interfaces.
 *
 * @param pSetup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return 0 on success, positive value if request is intended to be handled
 *	   by the core USB stack. Negative error code on fail.
 */
static int midi_custom_handler(struct usb_setup_packet *pSetup, int32_t *len,
				uint8_t **data)
{

}

/**
 * @brief Handler called for Class requests not handled by the USB stack.
 *
 * @param pSetup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail.
 */
static int midi_class_handle_req(struct usb_setup_packet *pSetup,
				  int32_t *len, uint8_t **data)
{
	
}

static int usb_midi_device_init(const struct device *dev)
{
}

#define DEFINE_MIDI_DEVICE(dev)					  \
	USBD_CFG_DATA_DEFINE(primary, midi)				  \
	struct usb_cfg_data midi_config_##dev = {			  \
		.usb_device_description	= NULL,				  \
		.interface_config = midi_interface_config,		  \
		.interface_descriptor = &midi_device_##dev.std_ac_interface, \
		.cb_usb_status = midi_cb_usb_status,			  \
		.interface = {						  \
			.class_handler = midi_class_handle_req,	  \
			.custom_handler = midi_custom_handler,		  \
			.vendor_handler = NULL,				  \
		},							  \
		.num_endpoints = ARRAY_SIZE(usb_midi_ep_data_##dev), \
		.endpoint = usb_midi_ep_data_##dev,		  \
	};								  \
	DEVICE_DT_DEFINE(DT_INST(dev, COMPAT_MIDI),			  \
			    &usb_midi_device_init,			  \
			    NULL,					  \
			    &midi_dev_data_##dev,			  \
			    &midi_config_##dev, APPLICATION,	  \
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  \
			    DUMMY_API)

#define MIDI_DEVICE(dev, _)     \
    DEFINE_MIDI_DEV_DATA(dev)   \
    DECLARE_MIDI_FUNCTION(dev)  \
    DEFINE_MIDI_DESCRIPTOR(dev) \
    DEFINE_EP_DATA(dev)         \
    DEFINE_MIDI_DEVICE(dev)     \

UTIL_LISTIFY(MIDI_DEVICE_COUNT, MIDI_DEVICE);