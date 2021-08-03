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
#include <usb/class/usb_midi.h>
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

static sys_slist_t usb_midi_data_devlist;

#define DEFINE_MIDI_DEV_DATA(dev)	\
	static struct usb_midi_dev_data midi_dev_data_##dev;





static void midi_interface_config(struct usb_desc_header *head,
				   uint8_t bInterfaceNumber)
{
	struct usb_if_descriptor *iface = (struct usb_if_descriptor *)head;
	struct cs_ac_if_descriptor *header;

#ifdef CONFIG_USB_COMPOSITE_DEVICE
	struct usb_association_descriptor *iad =
		(struct usb_association_descriptor *)
		((char *)iface - sizeof(struct usb_association_descriptor));
	iad->bFirstInterface = bInterfaceNumber;
#endif

	/* Audio Control Interface */
	iface->bInterfaceNumber = bInterfaceNumber;
	header = (struct cs_ac_if_descriptor *)
		 ((uint8_t *)iface + iface->bLength);

	
}

static void midi_cb_usb_status(struct usb_cfg_data *cfg,
			 enum usb_dc_status_code cb_status,
			 const uint8_t *param)
{	
}

// /**
//  * @brief Helper funciton for checking if particular entity is a part of
//  *	  the midi device.
//  *
//  * This function checks if given entity is a part of given midi device.
//  * If so then true is returned and midi_dev_data is considered correct device
//  * data.
//  *
//  * @note For now this function searches through feature units only. The
//  *	 descriptors are known and are not using any other entity type.
//  *	 If there is a need to add other units to midi function then this
//  *	 must be reworked.
//  *
//  * @param [in]      midi_dev_data USB midi device data.
//  * @param [in, out] entity	   USB midi entity.
//  *				   .id      [in]  id of searched entity
//  *				   .subtype [out] subtype of entity (if found)
//  *
//  * @return true if entity matched midi_dev_data, false otherwise.
//  */
// static bool is_entity_valid(struct usb_midi_dev_data *midi_dev_data,
// 			    struct usb_midi_entity *entity)
// {
// }

// /**
//  * @brief Helper funciton for getting the midi_dev_data by the entity number.
//  *
//  * This function searches through all midi devices the one with given
//  * entity number and return the midi_dev_data structure for this entity.
//  *
//  * @param [in, out] entity USB midi entity addressed by the request.
//  *			   .id      [in]  id of searched entity
//  *			   .subtype [out] subtype of entity (if found)
//  *
//  * @return midi_dev_data for given entity, NULL if not found.
//  */
// static struct usb_midi_dev_data *get_midi_dev_data_by_entity(
// 					struct usb_midi_entity *entity)
// {
// }

// /**
//  * @brief Helper funciton for checking if particular interface is a part of
//  *	  the midi device.
//  *
//  * This function checks if given interface is a part of given midi device.
//  * If so then true is returned and midi_dev_data is considered correct device
//  * data.
//  *
//  * @param [in] midi_dev_data USB midi device data.
//  * @param [in] interface      USB midi interface number.
//  *
//  * @return true if interface matched midi_dev_data, false otherwise.
//  */
// static bool is_interface_valid(struct usb_midi_dev_data *midi_dev_data,
// 			       uint8_t interface)
// {
// }

// /**
//  * @brief Helper funciton for getting the midi_dev_data by the interface
//  *	  number.
//  *
//  * This function searches through all midi devices the one with given
//  * interface number and returns the midi_dev_data structure for this device.
//  *
//  * @param [in] interface USB midi interface addressed by the request.
//  *
//  * @return midi_dev_data for given interface, NULL if not found.
//  */
// static struct usb_midi_dev_data *get_midi_dev_data_by_iface(uint8_t interface)
// {
// }

// /**
//  * @brief Handler for feature unit requests.
//  *
//  * This function handles feature unit specific requests.
//  * If request is properly served 0 is returned. Negative errno
//  * is returned in case of an error. This leads to setting stall on IN EP0.
//  *
//  * @param midi_dev_data USB midi device data.
//  * @param pSetup         Information about the executed request.
//  * @param len            Size of the buffer.
//  * @param data           Buffer containing the request result.
//  *
//  * @return 0 if succesfulf, negative errno otherwise.
//  */
// static int handle_feature_unit_req(struct usb_midi_dev_data *midi_dev_data,
// 				   struct usb_setup_packet *pSetup,
// 				   int32_t *len, uint8_t **data)
// {
// }

// /**
//  * @brief Handler called for class specific interface request.
//  *
//  * This function handles all class specific interface requests to a usb midi
//  * device. If request is properly server then 0 is returned. Returning negative
//  * value will lead to set stall on IN EP0.
//  *
//  * @param pSetup    Information about the executed request.
//  * @param len       Size of the buffer.
//  * @param data      Buffer containing the request result.
//  *
//  * @return  0 on success, negative errno code on fail.
//  */
// static int handle_interface_req(struct usb_setup_packet *pSetup,
// 				int32_t *len,
// 				uint8_t **data)
// {
	
// }

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
		LOG_INF("Init MIDI Device: dev %p (%s)", dev, dev->name);

		return 0;
}

// static void midi_write_cb(uint8_t ep, int size, void *priv)
// {
// }

// int usb_midi_send(const struct device *dev, struct net_buf *buffer,
// 		   size_t len)
// {
// }

// size_t usb_midi_get_in_frame_size(const struct device *dev)
// {
// }

static void midi_receive_cb(uint8_t ep, enum usb_dc_ep_cb_status_code status)
{
}

void usb_midi_register(const struct device *dev,
			const struct usb_midi_ops *ops)
{
	struct usb_midi_dev_data *midi_dev_data = dev->data;
	const struct usb_cfg_data *cfg = dev->config;
	const struct std_if_descriptor *iface_descr =
		cfg->interface_descriptor;
	const struct cs_ac_if_descriptor *header =
		(struct cs_ac_if_descriptor *)((uint8_t *)iface_descr);

	midi_dev_data->ops = ops;
	midi_dev_data->common.dev = dev;
	midi_dev_data->desc_hdr = header;

	sys_slist_append(&usb_midi_data_devlist, &midi_dev_data->common.node);

	LOG_INF("Device dev %p dev_data %p cfg %p added to devlist %p",
		dev, midi_dev_data, dev->config,
		&usb_midi_data_devlist);
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
	DEVICE_DT_DEFINE(DT_INST(dev, COMPAT_MIDI_DEVICE),			  \
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
    DEFINE_MIDI_EP_DATA(dev)         \
    DEFINE_MIDI_DEVICE(dev)  	\

UTIL_LISTIFY(MIDI_DEVICE_COUNT, MIDI_DEVICE);


void test_func(void) {
	//  struct cs_ms_if_descriptor test = INIT_CS_MS_IF(0, 0, 1);
    // LOG_INF("%s", STRINGIFY(UTIL_LISTIFY(MIDI_DEVICE_COUNT, MIDI_DEVICE, MIDI)));
	// uint16_t len = test.wTotalLength;
	// static uint8_t tester[] = { 1, 2, };
    // LOG_INF("%s", STRINGIFY((UTIL_LISTIFY(MIDI_DEVICE_COUNT, MIDI_DEVICE))));
	// LOG_INF("%s", TEST_STRING);
	LOG_INF("size: %d", (PROP_VAL_IS(ENDPOINT_N_ID(0, 0, 0, 1), direction, ENDPOINT_IN) << 7) |							
		DT_PROP(ENDPOINT_N_ID(0, 0, 0, 0), ep_num));
}