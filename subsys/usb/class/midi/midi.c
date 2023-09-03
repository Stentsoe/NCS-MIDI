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
#include <usb/usb_device.h>
#include <usb_descriptor.h>
#include <usb/class/usb_midi.h>
#include "usb_midi_internal.h"

#include "midi/midi.h"

#include <sys/byteorder.h>
#include <sys/util.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);

enum midi_revision {
	MIDI_1_0,
	MIDI_2_0,
};

struct midi_ep_data {
	uint8_t *cs_desc_ptr;
	uint8_t ep_num;
	uint8_t num_assoc;
	uint8_t *assoc;
	enum midi_revision rev; 
};

struct usb_midi_jack_pair_data {
	const struct usb_midi_jack_pair_config *cfg;

	uint8_t ep_num;

	const struct device *dev;

	sys_snode_t node;

	struct midi_api *api;

	void *user_data;

};
struct usb_midi_iface_data {
	bool enabled;
	uint8_t index;
};

/* Device data structure */
struct usb_midi_dev_data {
	const struct usb_midi_ops *ops;

	const struct cs_ac_if_descriptor *desc_hdr;

	struct usb_dev_data common;

	// struct net_buf_pool *pool;

	uint8_t num_ifaces;

	struct usb_midi_iface_data *iface_list;

	uint8_t num_eps;

	struct midi_ep_data *ep_list;

};

struct midi_ep_data *midi_internal_ep_data[16];

static sys_slist_t usb_midi_data_devlist;

static sys_slist_t usb_midi_jack_pair_list;

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
	/*	.pool = &buf_pool_##dev,*/ \
		.num_eps = DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_ENDPOINT), \
		.ep_list = midi_ep_list_##dev, \
	};

static inline const struct usb_midi_port_config *get_dev_config(const struct device *dev)
{
	return dev->config;
}

static void midi_interface_config(struct usb_desc_header *head,
				   uint8_t bInterfaceNumber)
{
	return;
}

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
{	//TODO: FIX THIS THING. GO THROUGH AUDIO SAMPLE AND UPDATE CHANGES.
	const struct cs_ac_if_descriptor *header;
	struct usb_midi_dev_data *midi_dev_data;
	struct usb_midi_iface_data *if_data = NULL;

	uint8_t iface = (pSetup->wIndex) & 0xFF;

	// LOG_INF("bmRT 0x%02x, bR 0x%02x, wV 0x%04x, wI 0x%04x, wL 0x%04x",
	// 	pSetup->bmRequestType, pSetup->bRequest, pSetup->wValue,
	// 	pSetup->wIndex, pSetup->wLength);
	if (pSetup->RequestType.recipient != USB_REQTYPE_RECIPIENT_INTERFACE ||
	    usb_reqtype_is_to_host(pSetup)) {
		return -EINVAL;
	}

	midi_dev_data = get_midi_dev_data_by_iface(iface);
	if (midi_dev_data == NULL) {
		return -EINVAL;
	}

	header = midi_dev_data->desc_hdr;

	

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
	case USB_SREQ_SET_INTERFACE:
		if_data->enabled = pSetup->wValue;
		return -EINVAL;
	case USB_SREQ_GET_INTERFACE:
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

// static struct usb_midi_jack_pair_data *get_midi_emb_jack_pair_by_in_id(uint8_t id)
// {
// 	struct usb_midi_jack_pair_data *jack_pair_data;

// 	SYS_SLIST_FOR_EACH_CONTAINER(&usb_midi_jack_pair_list, jack_pair_data, node) {
// 		if (jack_pair_data->cfg->id_in == id && jack_pair_data->cfg->type_in == EMBEDDED) {
// 			return jack_pair_data;
// 		}
// 	}
// 	return NULL;
// }

static struct usb_midi_jack_pair_data *get_midi_emb_jack_pair_by_in_cable_number(uint8_t cable_num)
{
	// struct usb_midi_jack_pair_data *jack_pair_data;
	static sys_snode_t *jack_pair_node;
	
	jack_pair_node = sys_slist_peek_head(&usb_midi_jack_pair_list);
	if(!jack_pair_node) {
		return NULL;
	}

	for (size_t i = 0; i < cable_num; i++)
	{
		jack_pair_node = sys_slist_peek_next_no_check(jack_pair_node);
	}

	return CONTAINER_OF(jack_pair_node, struct usb_midi_jack_pair_data, node);
}

static struct usb_midi_jack_pair_data *get_midi_emb_jack_pair_by_out_id(uint8_t id)
{
	struct usb_midi_jack_pair_data *jack_pair_data;
	SYS_SLIST_FOR_EACH_CONTAINER(&usb_midi_jack_pair_list, jack_pair_data, node) {
		if (jack_pair_data->cfg->id_out == id && jack_pair_data->cfg->type_out == EMBEDDED) {
			return jack_pair_data;
		}
	}
	return NULL;
}

struct midi_ep_data *get_ep_data_by_ep_num(struct usb_midi_dev_data *midi_dev_data, uint8_t ep_num)
{
	struct midi_ep_data *ep_data;

	for (size_t i = 0; i < midi_dev_data->num_eps; i++)
	{
		ep_data = (struct midi_ep_data *)((int)midi_dev_data->ep_list + i*sizeof(struct midi_ep_data));
		if(ep_data->ep_num == ep_num) 
		{
			return ep_data;
		}
	}	
	return NULL;
}

static int usb_midi_device_init(const struct device *dev)
{
	LOG_INF("Init MIDI Device: dev %p (%s)", dev, dev->name);

	struct usb_midi_dev_data *midi_dev_data = dev->data;
	struct midi_ep_data *ep_data;
	struct usb_midi_jack_pair_data *assoc_jack_pair;

	const struct usb_cfg_data *cfg = dev->config;
	const struct usb_if_descriptor *iface_descr =
		cfg->interface_descriptor;
	const struct cs_ac_if_descriptor *header =
		(struct cs_ac_if_descriptor *)((uint8_t *)iface_descr +
		iface_descr->bLength);
	
	midi_dev_data->common.dev = dev;
	midi_dev_data->desc_hdr = header;

	sys_slist_append(&usb_midi_data_devlist, &midi_dev_data->common.node);

	LOG_INF("Device dev %p dev_data %p cfg %p added to devlist %p",
		dev, midi_dev_data, dev->config,
		&usb_midi_data_devlist);

	for (size_t i = 0; i < midi_dev_data->num_eps; i++)
	{
		ep_data = (struct midi_ep_data *)((int)midi_dev_data->ep_list + i*sizeof(struct midi_ep_data));
		ep_data->ep_num = *(ep_data->cs_desc_ptr+2);
		if (ep_data->ep_num >> 7 && ep_data->rev == MIDI_1_0)
		{
			for (size_t i = 0; i < ep_data->num_assoc; i++)
			{	
				
				uint8_t jack_id = *(ep_data->assoc + i);

				// LOG_INF("jack: %d, ep_num: %d", jack_id, ep_data->ep_num);

				assoc_jack_pair = get_midi_emb_jack_pair_by_out_id(jack_id);
				if (assoc_jack_pair != NULL) {
					assoc_jack_pair->ep_num = ep_data->ep_num;
				} else {
					LOG_WRN("associated jack %d of endpoint %d not found", 
						jack_id, ep_data->ep_num);
				}
			}
		}
		LOG_INF("ep_data->ep_num %d", ep_data->ep_num);
	}
	return 0;
}

static int usb_midi_jack_pair_init(const struct device *dev) 
{
	struct usb_midi_jack_pair_data *jack_pair_data = dev->data;
	const struct usb_midi_jack_pair_config *cfg = dev->config;

	// jack_pair_data->id = cfg->id;
	jack_pair_data->dev = dev;
	jack_pair_data->cfg = cfg;
	jack_pair_data->api = (struct midi_api*)dev->api;

	sys_slist_append(&usb_midi_jack_pair_list, &jack_pair_data->node);

	LOG_INF("Init MIDI IN JACK: dev %p (%s)", dev, dev->name);

	return 0;
}

static int send_to_jack_pair(const struct device *dev,
				    midi_msg_t *msg,
					void *user_data)
{
	struct usb_midi_jack_pair_data *jack_pair_data = dev->data;
	uint32_t bytes_ret = 0;

	usb_write(jack_pair_data->ep_num, msg->data, msg->len, &bytes_ret);

	// LOG_INF("pointer %p, %p", jack_pair_data->api, jack_pair_data->api->midi_transfer_done);



	if (jack_pair_data->api && jack_pair_data->api->midi_transfer_done) 
	{
		jack_pair_data->api->midi_transfer_done(dev, msg, 
										jack_pair_data->user_data);
	} else {
		/* Release midi message back to the pool */
		midi_msg_unref(msg);
	}
	return 0;

}


static void midi_receive_cb(uint8_t ep, enum usb_dc_ep_cb_status_code status)
{
	struct usb_midi_dev_data *midi_dev_data;
	struct usb_dev_data *common;
	struct midi_ep_data *ep_data;
	struct usb_midi_jack_pair_data *jack_dev_data;
	midi_msg_t *msg;
	// uint8_t jack_id;
	uint8_t cable_number;
	int n_pending_bytes;
	int ret;
	LOG_INF("CB");
	common = usb_get_dev_data_by_ep(&usb_midi_data_devlist, ep);
	if (common == NULL) {
		return;
	}

	midi_dev_data = CONTAINER_OF(common, struct usb_midi_dev_data,
					common);

	ep_data = get_ep_data_by_ep_num(midi_dev_data, ep);
	
	// msg = midi_msg_alloc(4);

	LOG_INF("CB1");
	
	// TODO: Del opp jack pair, bruk cable number for å spore hvor cb skal sendes.

	ret = usb_read(ep, NULL, 0, &n_pending_bytes);
	LOG_INF("CB2 %d", n_pending_bytes);
	while (n_pending_bytes) {
		msg = midi_msg_alloc(NULL, 4);
		
		if (!msg->data) {
			LOG_ERR("Failed to allocate data buffer");
			return;
		}

		ret = usb_read(ep, msg->data, 4, &n_pending_bytes);
		if (ret) {
			LOG_ERR("ret=%d ", ret);
			midi_msg_unref(msg);
			return;
		}
		LOG_INF("CB3 %d", n_pending_bytes);

		if (!n_pending_bytes) {
			midi_msg_unref(msg);
			return;
		}
		
		LOG_INF("CB4 %d", n_pending_bytes);
		LOG_HEXDUMP_INF(msg->data, n_pending_bytes, "cb:");

		cable_number = (*msg->data) >> 4;
		jack_dev_data = get_midi_emb_jack_pair_by_in_cable_number(cable_number);
		
		LOG_INF("CB5 %d", n_pending_bytes);
		midi_send(jack_dev_data->dev, msg);
		
		LOG_INF("CB6 %d", n_pending_bytes);
		ret = usb_read(ep, NULL, 0, &n_pending_bytes);
	}
}

void usb_midi_register(const struct device *dev,
			const struct usb_midi_ops *ops)
{
	// struct usb_midi_dev_data *midi_dev_data = dev->data;
	// const struct usb_cfg_data *cfg = dev->config;
	// const struct usb_if_descriptor *iface_descr =
	// 	cfg->interface_descriptor;
	// const struct cs_ac_if_descriptor *header =
	// 	(struct cs_ac_if_descriptor *)((uint8_t *)iface_descr +
	// 	iface_descr->bLength);

	// midi_dev_data->ops = ops;
	// midi_dev_data->common.dev = dev;
	// midi_dev_data->desc_hdr = header;

	// sys_slist_append(&usb_midi_data_devlist, &midi_dev_data->common.node);

	// LOG_INF("Device dev %p dev_data %p cfg %p added to devlist %p",
	// 	dev, midi_dev_data, dev->config,
	// 	&usb_midi_data_devlist);
}

int usb_midi_jack_pair_callback_set(const struct device *dev,
				 midi_transfer cb,
				 void *user_data)
{
	struct usb_midi_jack_pair_data *jack_pair_data = dev->data;


	jack_pair_data->api->midi_transfer_done = cb; 
	jack_pair_data->user_data = user_data;

	return 0;
}

/*#define LIST_SOURCES_OF_OUT_JACK(idx, node_id)	\
	DT_PROP_BY_PHANDLE_IDX(node_id, sources, idx, id),	*/

#define DEFINE_MIDI_JACK_PAIR(jack, dev, iface, set, _) 								\
	struct usb_midi_jack_pair_data jack_pair_data_##dev##iface##set##jack;				\
	struct usb_midi_jack_pair_config jack_pair_cfg_data_##dev##iface##set##jack = {		\
		.id_out = DT_PROP(JACK_PAIR_N_ID(dev, iface, set, jack), id_out),				\
		.type_out = UTIL_COND_CHOICE_N(2,												\
			PROP_VAL_IS(JACK_PAIR_N_ID(dev, iface, set, jack), 							\
				type_out, JACK_EMBEDDED), 												\
			PROP_VAL_IS(JACK_PAIR_N_ID(dev, iface, set, jack), 							\
				type_out, JACK_EXTERNAL), 												\
		(EMBEDDED), (EXTERNAL)), 														\
		.id_in = DT_PROP(JACK_PAIR_N_ID(dev, iface, set, jack), id_in), 				\
		.type_in = UTIL_COND_CHOICE_N(2,												\
			PROP_VAL_IS(JACK_PAIR_N_ID(dev, iface, set, jack), 							\
				type_in, JACK_EMBEDDED), 												\
			PROP_VAL_IS(JACK_PAIR_N_ID(dev, iface, set, jack), 							\
				type_in, JACK_EXTERNAL), 												\
			(EMBEDDED), (EXTERNAL)),													\
	};																					\
	static struct midi_api usb_midi_jack_pair_api_##dev##iface##set##jack = {		\
		.midi_transfer = send_to_jack_pair, 												\
		.midi_callback_set = usb_midi_jack_pair_callback_set,							\
	};																				\
	DEVICE_DT_DEFINE(JACK_PAIR_N_ID(dev, iface, set, jack), 							\
			    &usb_midi_jack_pair_init,			  									\
			    NULL,					  											\
			    &jack_pair_data_##dev##iface##set##jack,			  					\
			    &jack_pair_cfg_data_##dev##iface##set##jack, 							\
				APPLICATION,	  													\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		  							\
			    &usb_midi_jack_pair_api_##dev##iface##set##jack);


#define DEFINE_MIDI_JACKS(dev)														\
	LISTIFY_ON_COMPAT_IN_DEVICE(DEFINE_MIDI_JACK_PAIR, COMPAT_MIDI_JACK_PAIR, dev)	\

/*#define DEFINE_BUF_POOL(dev) 														\
	NET_BUF_POOL_FIXED_DEFINE(buf_pool_##dev, 5, 8, 4, net_buf_destroy);
*/
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

#define LIST_ASSOC_JACK_OF_EP(idx, node_id)	\
		UTIL_COND_CHOICE_N(2, \
			PROP_VAL_IS(node_id, direction, ENDPOINT_IN),	\
			PROP_VAL_IS(node_id, direction, ENDPOINT_OUT),	\
			(DT_PROP_BY_PHANDLE_IDX(node_id, assoc_entities, idx, id_out),),\
			(DT_PROP_BY_PHANDLE_IDX(node_id, assoc_entities, idx, id_in),))													\
	

#define LIST_ASSOC_JACK_OF_IN_EP(idx, node_id)	\
	DT_PROP_BY_PHANDLE_IDX(node_id, assoc_entities, idx, id_out),

#define LIST_ASSOC_GT_OF_EP(idx, node_id)	\
	DT_PROP_BY_PHANDLE_IDX(node_id, assoc_entities, idx, id),

#define DEFINE_EP_ASSOC_LISTS(endpoint, dev, iface, set, _) 						\
	uint8_t ep_assoc_##dev##iface##set##endpoint[] = { UTIL_LISTIFY_LEVEL_4(		\
			DT_PROP_LEN(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities),	\
			UTIL_COND_CHOICE_N(2, 													\
				PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_2_0),	\
				PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_1_0),	\
				(LIST_ASSOC_GT_OF_EP),												\
				(LIST_ASSOC_JACK_OF_EP)), 											\
			ENDPOINT_N_ID(dev, iface, set, endpoint)) 								\
	}; 

	

#define INIT_ENDPOINTS(endpoint, dev, iface, set, _) 										\
	{																						\
		.cs_desc_ptr = (uint8_t*)&midi_device_##dev.std_ep_##iface##set##endpoint,			\
		.num_assoc = DT_PROP_LEN(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities), \
		.assoc = ep_assoc_##dev##iface##set##endpoint, 										\
		.rev = UTIL_COND_CHOICE_N(2, \
			PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_1_0),\
			PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_2_0),\
			(MIDI_1_0), (MIDI_2_0))\
	},

#define DEFINE_MIDI_EP_DATA(dev)												\
	static struct usb_ep_cfg_data usb_midi_ep_data_##dev[] = {					\
		LISTIFY_ON_COMPAT_IN_DEVICE(LIST_ENDPOINTS, COMPAT_MIDI_ENDPOINT, dev)	\
	}; 																				\
	 																				\
	LISTIFY_ON_COMPAT_IN_DEVICE(DEFINE_EP_ASSOC_LISTS, COMPAT_MIDI_ENDPOINT, dev)	\
	struct midi_ep_data midi_ep_list_##dev[] = {									\
		LISTIFY_ON_COMPAT_IN_DEVICE(INIT_ENDPOINTS, COMPAT_MIDI_ENDPOINT, dev)		\
	};																				\
	

#define MIDI_DEVICE(dev, _)     \
	/*DEFINE_BUF_POOL(dev) */   \
	DECLARE_MIDI_FUNCTION(dev)  \
	DEFINE_MIDI_DESCRIPTOR(dev) \
	DEFINE_MIDI_EP_DATA(dev)    \
	DEFINE_MIDI_DEV_DATA(dev)   \
	DEFINE_MIDI_DEVICE(dev)   	\
	DEFINE_MIDI_JACKS(dev) 

UTIL_LISTIFY(USB_MIDI_DEVICE_COUNT, MIDI_DEVICE);

#define	TEST_STRING					\
	STRINGIFY(						\
		(DEFINE_MIDI_DESCRIPTOR(0))	\
	)

void test_func(void) {
	
		// LOG_INF("%s", TEST_STRING);
	
}