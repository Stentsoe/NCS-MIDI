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
#include "drivers/midi.h"

#include <sys/byteorder.h>
#include <sys/util.h>
#include <net/buf.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);



// struct usb_midi_group_terminal_block {
// 	uint8_t id;
// };


struct usb_midi_out_jack_data {
	uint8_t id;
	struct midi_api *api;
	sys_snode_t node;
};

struct usb_midi_in_jack_data {
	uint8_t id;
	sys_slist_t api_list;
	sys_snode_t node;
};


struct midi_ep_data {
	uint8_t *cs_desc_ptr;
	uint8_t ep_num;
	uint8_t num_assoc;
	uint8_t *assoc;
};


struct usb_midi_iface_data {
	bool enabled;
	uint8_t index;
	// uint8_t num_ext_jacks;
	// struct usb_midi_ext_jack *ext_jacks;
};

/* Device data structure */
struct usb_midi_dev_data {
	const struct usb_midi_ops *ops;

	const struct cs_ac_if_descriptor *desc_hdr;

	struct usb_dev_data common;

	struct net_buf_pool *pool;

	uint8_t num_ifaces;

	struct usb_midi_iface_data *iface_list;

	uint8_t num_eps;

	struct midi_ep_data *ep_list;

};

struct midi_ep_data *midi_internal_ep_data[16];

static sys_slist_t usb_midi_data_devlist;

static sys_slist_t usb_midi_in_jack_list;


#define INIT_JACK_DATA(jack, dev, iface, set, _) \
	{}

#define DEFINE_JACK_DATA(iface, dev, _)	\
	struct usb_midi_ext_jack jack_data_##dev##iface[] = {		\
			LISTIFY_ON_COMPAT_IN_SETTING(						\
			INIT_JACK_DATA, COMPAT_MIDI_IN_JACK, dev)		\
	};

#define INIT_IFACE_DATA(iface, dev, _) 	\
	{								\
		.enabled = false,			\
		.index = DT_PROP(IFACE_N_ID(dev, iface), index), \
	},

#define DEFINE_MIDI_DEV_DATA(dev)							\
	struct usb_midi_iface_data iface_data_##dev[] = {		\
		LISTIFY_ON_COMPAT_IN_DEVICE(						\
		INIT_IFACE_DATA, COMPAT_MIDI_INTERFACE, dev)		\
	};														\
	static struct usb_midi_dev_data midi_dev_data_##dev = {	\
		.iface_list = iface_data_##dev,						\
		.num_ifaces = COMPAT_COUNT(DEV_N_ID(dev), COMPAT_MIDI_INTERFACE), \
		.pool = &buf_pool_##dev, \
		.num_eps = DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_ENDPOINT), \
		.ep_list = midi_ep_list_##dev, \
	};


static inline const struct usb_midi_port_config *get_dev_config(const struct device *dev)
{
	return dev->config;
}



static void midi_interface_config(struct usb_desc_header *head,
				   uint8_t bInterfaceNumber);


static void midi_cb_usb_status(struct usb_cfg_data *cfg,
			 enum usb_dc_status_code cb_status,
			 const uint8_t *param)
{
	switch (cb_status)
	{
	case USB_DC_ERROR:
		LOG_INF("USB_DC_ERROR");
		break;
	case USB_DC_RESET:
		LOG_INF("USB_DC_RESET");
		break;
	case USB_DC_CONNECTED:
		LOG_INF("USB_DC_CONNECTED");
		break;
	case USB_DC_CONFIGURED:
		LOG_INF("USB_DC_CONFIGURED");
		break;
	case USB_DC_DISCONNECTED:
		LOG_INF("USB_DC_DISCONNECTED");
		break;
	case USB_DC_SUSPEND:
		LOG_INF("USB_DC_SUSPEND");
		break;
	case USB_DC_RESUME:
		LOG_INF("USB_DC_RESUME");
		break;
	case USB_DC_INTERFACE:
		LOG_INF("USB_DC_INTERFACE");
		break;
	case USB_DC_SET_HALT:
		LOG_INF("USB_DC_SET_HALT");
		break;
	case USB_DC_CLEAR_HALT:
		LOG_INF("USB_DC_CLEAR_HALT");
		break;
	case USB_DC_SOF:
		LOG_INF("USB_DC_SOF");
		break;

	default:
		break;
	}
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

/**
 * @brief Helper funciton for checking if particular interface is a part of
 *	  the midi device.
 *
 * This function checks if given interface is a part of given midi device.
 * If so then true is returned and midi_dev_data is considered correct device
 * data.
 *
 * @param [in] midi_dev_data USB midi device data.
 * @param [in] interface      USB midi interface number.
 *
 * @return true if interface matched midi_dev_data, false otherwise.
 */
static bool is_interface_valid(struct usb_midi_dev_data *midi_dev_data,
			       uint8_t interface)
{
	const struct cs_ac_if_descriptor *header;



	header = midi_dev_data->desc_hdr;

	uint8_t desc_iface = 0;
	for (size_t i = 0; i < header->bInCollection; i++) {
		desc_iface = header->baInterfaceNr[i];
		if (desc_iface == interface) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Helper funciton for getting the midi_dev_data by the interface
 *	  number.
 *
 * This function searches through all midi devices the one with given
 * interface number and returns the midi_dev_data structure for this device.
 *
 * @param [in] interface USB midi interface addressed by the request.
 *
 * @return midi_dev_data for given interface, NULL if not found.
 */
static struct usb_midi_dev_data *get_midi_dev_data_by_iface(uint8_t interface)
{
	struct usb_dev_data *dev_data;
	struct usb_midi_dev_data *midi_dev_data;

	SYS_SLIST_FOR_EACH_CONTAINER(&usb_midi_data_devlist, dev_data, node) {
		midi_dev_data = CONTAINER_OF(dev_data,
					      struct usb_midi_dev_data,
					      common);
		if (is_interface_valid(midi_dev_data, interface)) {
			return midi_dev_data;
		}
	}
	return NULL;
}

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

// 	LOG_INF("req: %d", pSetup->bmRequestType);
// 	return 0;
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
	// const struct cs_ac_if_descriptor *header;
	struct usb_midi_dev_data *midi_dev_data;
	struct usb_midi_iface_data *if_data = NULL;

	uint8_t iface = (pSetup->wIndex) & 0xFF;

	// LOG_INF("bmRT 0x%02x, bR 0x%02x, wV 0x%04x, wI 0x%04x, wL 0x%04x",
	// 	pSetup->bmRequestType, pSetup->bRequest, pSetup->wValue,
	// 	pSetup->wIndex, pSetup->wLength);
	if (REQTYPE_GET_RECIP(pSetup->bmRequestType) != REQTYPE_RECIP_INTERFACE) {
		return -ENOTSUP;
	}

	midi_dev_data = get_midi_dev_data_by_iface(iface);
	if (midi_dev_data == NULL) {
		return -EINVAL;
	}


	// LOG_INF("iface %d, %d", iface, midi_dev_data->num_ifaces);

	for (size_t i = 0; i < midi_dev_data->num_ifaces; i++)
	{
		if(midi_dev_data->iface_list[i].index == iface)
		{
			// LOG_INF("midi_dev_data->iface_list[i].index %d", midi_dev_data->iface_list[i].index);
			if_data = &midi_dev_data->iface_list[i];
		}
	}

	if (if_data == NULL) {
		return -EINVAL;
	}

	switch (pSetup->bRequest) {
	case REQ_SET_INTERFACE:
		if_data->enabled = pSetup->wValue;
		return -EINVAL;
	case REQ_GET_INTERFACE:
		*data[0] = if_data->enabled;
		return 0;
	default:
		break;
	}

	return -ENOTSUP;
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
	LOG_INF("midi_class_handle_req!!!!!!!!!!!!!!!!!");
	// LOG_DBG("bmRT 0x%02x, bR 0x%02x, wV 0x%04x, wI 0x%04x, wL 0x%04x",
	// 	pSetup->bmRequestType, pSetup->bRequest, pSetup->wValue,
	// 	pSetup->wIndex, pSetup->wLength);
	return 0;
}

	

struct midi_ep_data *get_ep_data_by_ep_num(uint8_t ep_num)
{
	struct usb_dev_data *dev_data;
	struct usb_midi_dev_data *midi_dev_data;
	struct midi_ep_data *ep_data;

	SYS_SLIST_FOR_EACH_CONTAINER(&usb_midi_data_devlist, dev_data, node) {
		midi_dev_data = CONTAINER_OF(dev_data,
					      struct usb_midi_dev_data,
					      common);
		for (size_t i = 0; i < midi_dev_data->num_eps; i++)
		{
			ep_data = (struct midi_ep_data *)((int)midi_dev_data->ep_list + i*sizeof(struct midi_ep_data));
			if(ep_data->ep_num == ep_num) {
				return ep_data;
			}
		}	
	}
	return NULL;
}


static int usb_midi_device_init(const struct device *dev)
{
		LOG_INF("Init MIDI Device: dev %p (%s)", dev, dev->name);

		struct usb_midi_dev_data *midi_dev_data = dev->data;
		struct midi_ep_data *ep_data;

		
		for (size_t i = 0; i < midi_dev_data->num_eps; i++)
		{
			ep_data = (struct midi_ep_data *)((int)midi_dev_data->ep_list + i*sizeof(struct midi_ep_data));
			ep_data->ep_num = *(ep_data->cs_desc_ptr+2);
			
			LOG_INF("ep_data->ep_num %d", ep_data->ep_num);
		}
		return 0;
}

static struct usb_midi_in_jack_data *get_midi_in_jack_by_id(uint8_t id)
{
	struct usb_midi_in_jack_data *in_jack_data;

	SYS_SLIST_FOR_EACH_CONTAINER(&usb_midi_in_jack_list, in_jack_data, node) {
		if (in_jack_data->id == id) {
			return in_jack_data;
		}
	}
	return NULL;

}

static int usb_midi_out_jack_init(const struct device *dev) 
{

	struct usb_midi_out_jack_data *out_jack_data = dev->data;
	const struct usb_midi_out_jack_config *cfg = dev->config;
	uint8_t *source = cfg->sources;
	struct usb_midi_in_jack_data *in_jack_data;

	out_jack_data->api = (struct midi_api*)dev->api;
	out_jack_data->id = cfg->id;

	for (size_t i = 0; i < cfg->num_sources; i++)
	{
		in_jack_data = get_midi_in_jack_by_id((uint8_t)*(source + i));
		if(in_jack_data) {
			sys_slist_append(&in_jack_data->api_list, &out_jack_data->node);
		}
		in_jack_data = NULL;
	}
	
	LOG_INF("Init MIDI OUT JACK: dev %p (%s)", dev, dev->name);

	return 0;
}

static int usb_midi_in_jack_init(const struct device *dev) 
{
	struct usb_midi_in_jack_data *in_jack_data = dev->data;
	const struct usb_midi_in_jack_config *cfg = dev->config;

	in_jack_data->id = cfg->id;

	sys_slist_append(&usb_midi_in_jack_list, &in_jack_data->node);

	LOG_INF("Init MIDI IN JACK: dev %p (%s)", dev, dev->name);

	return 0;
}

// static void midi_write_cb(uint8_t ep, int size, void *priv)
// {
// }

// int usb_midi_send(const struct device *dev, struct net_buf *buffer,
// 		   size_t len)
// {
	// struct usb_midi_dev_data *midi_dev_data = dev->data;
	// struct usb_cfg_data *cfg = (void *)dev->config;
	// /* EP ISO IN is always placed first in the endpoint table */
	// uint8_t ep = cfg->endpoint[0].ep_addr;

	// if (!(ep & USB_EP_DIR_MASK)) {
	// 	LOG_ERR("Wrong device");
	// 	return -EINVAL;
	// }

	// // if (!midi_dev_data->tx_enable) {
	// // 	LOG_DBG("sending dropped -> Host chose passive interface");
	// // 	return -EAGAIN;
	// // }

	// if (len > buffer->size) {
	// 	LOG_ERR("Cannot send %d bytes, to much data", len);
	// 	return -EINVAL;
	// }

	// /** buffer passed to *priv because completion callback
	//  * needs to release it to the pool
	//  */
	// usb_transfer(ep, buffer->data, len, USB_TRANS_WRITE | USB_TRANS_NO_ZLP,
	// 	     midi_write_cb, buffer);
// 	return 0;
// }




static void send_to_in_jack(struct usb_midi_in_jack_data *in_jack_data,
					const struct device *dev,
				    struct net_buf *buffer,
					size_t size)
{
	struct usb_midi_out_jack_data *out_jack_data;
	struct midi_api *api;
	SYS_SLIST_FOR_EACH_CONTAINER(&in_jack_data->api_list, out_jack_data, node) {
		api = out_jack_data->api;
		if (api != NULL && api->midi_transfer != NULL) {
			api->midi_transfer(dev, buffer, size);
		}
	}
}


static void midi_receive_cb(uint8_t ep, enum usb_dc_ep_cb_status_code status)
{

	struct midi_ep_data *ep_data;
	struct usb_midi_in_jack_data *jack;
	uint8_t jack_id;
	struct net_buf *buffer;
	int ret_bytes;
	int ret;

	// TODO: finne ut hvordan enten få tak i pool, eller definere pool per ep. Samt finne ut hvordan device skal returneres i callback.
	
	ep_data = get_ep_data_by_ep_num(ep);
	buffer = net_buf_alloc(midi_dev_data->pool, K_NO_WAIT);
	if (!buffer) {
		LOG_ERR("Failed to allocate data buffer");
		return;
	}

	ret = usb_read(ep, buffer->data, buffer->size, &ret_bytes);

	if (ret) {
		LOG_ERR("ret=%d ", ret);
		net_buf_unref(buffer);
		return;
	}

	if (!ret_bytes) {
		net_buf_unref(buffer);
		return;
	}

	LOG_INF("ep_num %d", ep_data->ep_num);
	for (size_t i = 0; i < ep_data->num_assoc; i++)
	{
		jack_id = *(uint8_t*)((int)ep_data->assoc+i);
		LOG_INF("ep_num %d", jack_id);
		jack = get_midi_in_jack_by_id(jack_id);
		send_to_in_jack(jack, NULL, net_buf_clone(buffer, K_NO_WAIT), ret_bytes);
	}
	
	net_buf_unref(buffer);
	// struct usb_midi_dev_data *midi_dev_data;
	// struct usb_dev_data *common;
	// struct net_buf *buffer;
	// struct usb_midi_emb_jack *emb_jack;
	// int ret_bytes;
	// int ret;

	// __ASSERT(status == USB_DC_EP_DATA_OUT, "Invalid ep status");

	// LOG_INF("Received data on endpoint %d", ep);

	// buffer = net_buf_alloc(midi_dev_data->pool, K_NO_WAIT);
	// if (!buffer) {
	// 	LOG_ERR("Failed to allocate data buffer");
	// 	return;
	// }

	// ret = usb_read(ep, buffer->data, buffer->size, &ret_bytes);

	// if (ret) {
	// 	LOG_ERR("ret=%d ", ret);
	// 	net_buf_unref(buffer);
	// 	return;
	// }

	// if (!ret_bytes) {
	// 	net_buf_unref(buffer);
	// 	return;
	// }


	// for (size_t i = 0; i < midi_internal_ep_data[ep].num_assoc; i++)
	// {
	// 	emb_jack = (struct usb_midi_emb_jack*) midi_internal_ep_data[ep].assoc.in_jacks + i * sizeof(struct usb_midi_emb_jack);
	// 	if (emb_jack == NULL)
	// 	{
	// 		LOG_INF("Received data on endpoint with no embedded jacks");
	// 		return;
	// 	}

	// 	emb_jack->received_cb(common->dev, net_buf_clone(buffer, K_NO_WAIT), ret_bytes);
	// }

	// net_buf_unref(buffer);


	// }
}

void usb_midi_register(const struct device *dev,
			const struct usb_midi_ops *ops)
{
	struct usb_midi_dev_data *midi_dev_data = dev->data;
	const struct usb_cfg_data *cfg = dev->config;
	const struct usb_if_descriptor *iface_descr =
		cfg->interface_descriptor;
	const struct cs_ac_if_descriptor *header =
		(struct cs_ac_if_descriptor *)((uint8_t *)iface_descr +
		iface_descr->bLength);

	LOG_INF(" header %p", header);
	midi_dev_data->ops = ops;
	midi_dev_data->common.dev = dev;
	midi_dev_data->desc_hdr = header;

	sys_slist_append(&usb_midi_data_devlist, &midi_dev_data->common.node);

	// LOG_INF("Device dev %p dev_data %p cfg %p added to devlist %p",
	// 	dev, midi_dev_data, dev->config,
	// 	&usb_midi_data_devlist);
}

int usb_midi_send(const struct device *dev,
				    struct net_buf *buffer,
					size_t size)
{
	return 0;
}

// void usb_midi_callback_set(const struct device *dev,
// 				 struct usb_midi_ops cb,
// 				 void *user_data)
// {
// 	return;
// }

int usb_midi_out_jack_callback_set(const struct device *dev,
				 midi_transfer cb,
				 void *user_data)
{
	const struct usb_midi_out_jack_config *cfg = dev->config;
	struct usb_midi_out_jack_data *out_jack_data = dev->data;

	if (cfg->type == EMBEDDED)
	{	
		return -EINVAL;
	}

	out_jack_data->api->midi_transfer = cb; 

	LOG_INF("callback set %p", out_jack_data->api->midi_transfer);
	
	return 0;
}


#define LIST_SOURCES_OF_OUT_JACK(idx, node_id)	\
	DT_PROP_BY_PHANDLE_IDX(node_id, sources, idx, id),	

#define DEFINE_MIDI_OUT_JACK(jack, dev, iface, set, _)								\
	uint8_t out_jack_sources_##dev##iface##set##jack[] = { UTIL_LISTIFY_LEVEL_4(		\
			DT_PROP_LEN(OUT_JACK_N_ID(dev, iface, set, jack), sources),				\
			LIST_SOURCES_OF_OUT_JACK, OUT_JACK_N_ID(dev, iface, set, jack)) };		\
	struct usb_midi_out_jack_data out_jack_data_##dev##iface##set##jack;			\
	struct usb_midi_out_jack_config out_jack_cfg_data_##dev##iface##set##jack = {	\
		.id = DT_PROP(OUT_JACK_N_ID(dev, iface, set, jack), id),					\
		.type = UTIL_COND_CHOICE_N(2,												\
		PROP_VAL_IS(OUT_JACK_N_ID(dev, iface, set, jack), 							\
			type, JACK_EMBEDDED), 													\
		PROP_VAL_IS(OUT_JACK_N_ID(dev, iface, set, jack), 							\
			type, JACK_EXTERNAL), 													\
		(EMBEDDED), (EXTERNAL)) ,													\
		.num_sources = DT_PROP_LEN(OUT_JACK_N_ID(dev, iface, set, jack), sources),	\
		.sources = out_jack_sources_##dev##iface##set##jack, 						\
	};																				\
	static struct midi_api usb_midi_out_jack_api_##dev##iface##set##jack = {	\
		.midi_callback_set = usb_midi_out_jack_callback_set, 						\
	};																				\
																					\
	DEVICE_DT_DEFINE(OUT_JACK_N_ID(dev, iface, set, jack), 							\
			    &usb_midi_out_jack_init,			  									\
			    NULL,					  											\
			    &out_jack_data_##dev##iface##set##jack,			  					\
			    &out_jack_cfg_data_##dev##iface##set##jack, 						\
				APPLICATION,	  													\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  							\
			    &usb_midi_out_jack_api_##dev##iface##set##jack);


#define DEFINE_MIDI_IN_JACK(jack, dev, iface, set, _) 								\
	struct usb_midi_in_jack_data in_jack_data_##dev##iface##set##jack;				\
	struct usb_midi_in_jack_config in_jack_cfg_data_##dev##iface##set##jack = {		\
		.id = DT_PROP(IN_JACK_N_ID(dev, iface, set, jack), id),						\
		.type = UTIL_COND_CHOICE_N(2,												\
		PROP_VAL_IS(IN_JACK_N_ID(dev, iface, set, jack), 							\
			type, JACK_EMBEDDED), 													\
		PROP_VAL_IS(IN_JACK_N_ID(dev, iface, set, jack), 							\
			type, JACK_EXTERNAL), 													\
		(EMBEDDED), (EXTERNAL)), 													\
	};																				\
	DEVICE_DT_DEFINE(IN_JACK_N_ID(dev, iface, set, jack), 							\
			    &usb_midi_in_jack_init,			  									\
			    NULL,					  											\
			    &in_jack_data_##dev##iface##set##jack,			  					\
			    &in_jack_cfg_data_##dev##iface##set##jack, 							\
				APPLICATION,	  													\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  							\
			    DUMMY_API);

#define DEFINE_MIDI_JACKS(dev)	\
	LISTIFY_ON_COMPAT_IN_DEVICE(DEFINE_MIDI_OUT_JACK, COMPAT_MIDI_OUT_JACK, dev)\
	LISTIFY_ON_COMPAT_IN_DEVICE(DEFINE_MIDI_IN_JACK, COMPAT_MIDI_IN_JACK, dev) \

#define DEFINE_BUF_POOL(dev) \
	NET_BUF_POOL_FIXED_DEFINE(buf_pool_##dev, 5, 8, net_buf_destroy);

#define DEFINE_EMB_IN_JACK(jack, dev, iface, set, _) 						\
	struct usb_midi_emb_jack emb_in_jack_##dev##iface##set##jack = { 	\
		.id = DT_PROP(IN_JACK_N_ID(dev, iface, set, jack), id) 			\
	};

#define DEFINE_MIDI_EMB_JACKS(dev)\
	struct usb_midi_emb_jack *emb_in_jacks[DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_IN_JACK)]; \
	LISTIFY_ON_COMPAT_IN_DEVICE(DEFINE_EMB_IN_JACK, COMPAT_MIDI_IN_JACK, dev)

#define DEFINE_MIDI_DEVICE(dev)					  				\
	USBD_CFG_DATA_DEFINE(primary, midi)				  			\
	struct usb_cfg_data midi_config_##dev = {			  		\
		.usb_device_description	= NULL,				  			  		\
		.interface_config = midi_interface_config,		 	 	    	\
		.interface_descriptor = &midi_device_##dev.std_ac_interface, 	\
		.cb_usb_status = midi_cb_usb_status,			  		    	\
		.interface = {						  					   		\
			.class_handler = midi_class_handle_req,	  			 		\
			.custom_handler = midi_custom_handler,		  	    \
			.vendor_handler = NULL,				  				\
		},							  							\
		.num_endpoints = ARRAY_SIZE(usb_midi_ep_data_##dev), 	\
		.endpoint = usb_midi_ep_data_##dev,		  				\
	};								  							\
	DEVICE_DT_DEFINE(DEV_N_ID(dev),			  					\
			    &usb_midi_device_init,			  				\
			    NULL,					  						\
			    &midi_dev_data_##dev,			  				\
			    &midi_config_##dev, APPLICATION,	  			\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  		\
			    DUMMY_API);

#define LIST_ENDPOINTS(endpoint, dev, iface, set, _) \
	UTIL_COND_CHOICE_N(2, \
		PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), direction, ENDPOINT_IN),\
		PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), direction, ENDPOINT_OUT),\
		(INIT_EP_DATA(usb_transfer_ep_callback, AUTO_EP_IN), ), \
		(INIT_EP_DATA(midi_receive_cb, AUTO_EP_OUT), ))

#define LIST_ASSOC_OF_OUT_EP(idx, node_id)	\
	DT_PROP_BY_PHANDLE_IDX(node_id, assoc_entities, idx, id),	

#define DEFINE_EP_ASSOC_LISTS(endpoint, dev, iface, set, _) \
	uint8_t ep_assoc_##dev##iface##set##endpoint[] = { UTIL_LISTIFY_LEVEL_4(		\
			DT_PROP_LEN(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities),				\
			LIST_ASSOC_OF_OUT_EP, ENDPOINT_N_ID(dev, iface, set, endpoint)) \
	}; 


#define LIST_ENPOINTS(endpoint, dev, iface, set, _) 					\
	{																						\
		.cs_desc_ptr = (uint8_t*)&midi_device_##dev.std_ep_##iface##set##endpoint,						\
		.num_assoc = DT_PROP_LEN(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities), \
		.assoc = ep_assoc_##dev##iface##set##endpoint, 										\
	},

#define DEFINE_MIDI_EP_DATA(dev)							\
	static struct usb_ep_cfg_data usb_midi_ep_data_##dev[] = {	\
		LISTIFY_ON_COMPAT_IN_DEVICE(LIST_ENDPOINTS, COMPAT_MIDI_ENDPOINT, dev)\
	}; \
	 \
	LISTIFY_ON_COMPAT_IN_DEVICE(DEFINE_EP_ASSOC_LISTS, COMPAT_MIDI_ENDPOINT, dev)\
	struct midi_ep_data midi_ep_list_##dev[] = {\
		LISTIFY_ON_COMPAT_IN_DEVICE(LIST_ENPOINTS, COMPAT_MIDI_ENDPOINT, dev)\
	};
	

#define MIDI_DEVICE(dev, _)     \
	DEFINE_BUF_POOL(dev)        \
	DECLARE_MIDI_FUNCTION(dev)  \
	DEFINE_MIDI_DESCRIPTOR(dev) \
	DEFINE_MIDI_EP_DATA(dev)    \
	DEFINE_MIDI_DEV_DATA(dev)   \
	DEFINE_MIDI_DEVICE(dev)     \
	DEFINE_MIDI_JACKS(dev)

UTIL_LISTIFY(MIDI_DEVICE_COUNT, MIDI_DEVICE);

#define	TEST_STRING	\
	STRINGIFY(\
		(DEFINE_MIDI_EMB_JACKS(0))\
	)

void test_func(void) {
    LOG_INF("%s", TEST_STRING);
}


static void midi_interface_config(struct usb_desc_header *head,
				   uint8_t bInterfaceNumber)
{

	
}



static int usb_midi_cfg_init(const struct device *arg)
{
	return 0;
}


SYS_INIT(usb_midi_cfg_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
