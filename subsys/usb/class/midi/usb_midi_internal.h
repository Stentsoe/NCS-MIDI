
/**
 * @file
 * @brief USB MIDI Device Class internal header
 *
 * This header file is used to store internal configuration
 * defines.
 */

#include "usb/class/usb_midi.h"
#include "sys/util_macro_expansion.h"
#include <sys/util_internal.h>

#ifndef ZEPHYR_INCLUDE_USB_CLASS_MIDI_INTERNAL_H_
#define ZEPHYR_INCLUDE_USB_CLASS_MIDI_INTERNAL_H_

#define DUMMY_API  (const void *)1

#define USB_AC_IFACE_DESC_SIZE(dev)	 \
	sizeof(struct cs_ac_if_descriptor_##dev)



/* Names of compatibles used for configuration of the device */
#define COMPAT_MIDI_USB_DEVICE  			usb_midi_device
#define COMPAT_MIDI_INTERFACE				usb_midi_interface
#define COMPAT_MIDI_SETTING  				usb_midi_setting
#define COMPAT_MIDI_JACK_PAIR  				usb_midi_jack_pair
#define COMPAT_MIDI_ENDPOINT				usb_midi_endpoint
#define COMPAT_MIDI_GT_BLK  	usb_midi_group_terminal_block

/* Enums defined in devicetree */
#define SETTING_MIDI_1_0		0
#define SETTING_MIDI_2_0		1

#define	JACK_EMBEDDED	0
#define	JACK_EXTERNAL	1

#define GT_BLK_IN		0
#define GT_BLK_OUT		1
#define	GT_BLK_IN_OUT	2

#define GT_BLK_UNKNOWN					0
#define GT_BLK_MIDI_1_0_64				1
#define GT_BLK_MIDI_1_0_64_TIMESTAMPS	2
#define GT_BLK_MIDI_1_0_128				3
#define GT_BLK_MIDI_1_0_128_TIMESTAMPS	4
#define GT_BLK_MIDI_2_0					5
#define GT_BLK_MIDI_2_0_TIMESTAMPS		6

#define ENDPOINT_IN		0
#define ENDPOINT_OUT 	1

#define ENDPOINT_BULK			0
#define ENDPOINT_INTERRUPT		1

#define NODE_LIST(node_id) LIST(node_id, _)

#define COMPAT_LIST(i, compat) 					\
	COND_CODE_1(DT_NODE_HAS_COMPAT(i, compat), (i), ()),

/* List of children with a given compatible of a node*/
#define COMPAT_CHILDREN_LIST(node_id, compat)	\
	LIST_DROP_EMPTY(FOR_EACH_FIXED_ARG(			\
			COMPAT_LIST, (), compat, DT_FOREACH_CHILD(node_id, NODE_LIST)))

/* Number of children with a given compatible of a node*/
#define COND_NODE_HAS_COMPAT_CHILD(node_id, compat, if_code, else_code)	\
	COND_CODE_1(DT_NODE_EXISTS(GET_ARG_N(1, COMPAT_CHILDREN_LIST(node_id, compat))),	\
	 if_code, else_code)

/* Number of children with a given compatible of a node*/
#define COMPAT_COUNT(node_id, compat)	\
	COND_NODE_HAS_COMPAT_CHILD(node_id, compat, \
		(NUM_VA_ARGS_LESS_1(COMPAT_CHILDREN_LIST(node_id, compat),)), (0))

/* Get MIDI device node ID */
#define DEV_N_ID(dev)	DT_INST(dev, COMPAT_MIDI_USB_DEVICE)

/* Get MIDI setting node ID */
#define IFACE_N_ID(dev, iface)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(iface), COMPAT_CHILDREN_LIST(		\
		DEV_N_ID(dev), COMPAT_MIDI_INTERFACE))

/* Get MIDI interface node ID */
#define SET_N_ID(dev, iface, set)						\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(set), COMPAT_CHILDREN_LIST( 	\
	IFACE_N_ID(dev, iface), COMPAT_MIDI_SETTING))

/* Get MIDI jack pair out node ID */
#define JACK_PAIR_N_ID(dev, iface, set, jack)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(jack), COMPAT_CHILDREN_LIST(		\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_JACK_PAIR))

/* Get MIDI group terminal node ID */
#define GT_BLK_N_ID(dev, iface, set, gt_blk)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(gt_blk), COMPAT_CHILDREN_LIST(		\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK))

/* Get MIDI in jack node ID */
#define ENDPOINT_N_ID(dev, iface, set, endpoint)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(endpoint), COMPAT_CHILDREN_LIST(		\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT))

/* Number of MIDI devices*/
#define USB_MIDI_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_USB_DEVICE)

/* Perform UTIL_LISTIFY() on all compatible children of an interface*/
#define	LISTIFY_ON_COMPAT_IN_SETTING(set, F, compat, dev, iface, ...)	\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(						\
		SET_N_ID(dev, iface, set), compat), 	\
		F, dev, iface, set, __VA_ARGS__)

/* Perform UTIL_LISTIFY() on all downstream compatibles of an alternate setting*/
#define	LISTIFY_ON_COMPAT_IN_INTERFACE(iface, F, compat, dev, ...)	\
UTIL_LISTIFY_LEVEL_2(COMPAT_COUNT(						\
	IFACE_N_ID(dev, iface), compat), 				\
	F, dev, iface, __VA_ARGS__)									\
UTIL_LISTIFY_LEVEL_2(COMPAT_COUNT(IFACE_N_ID(dev, iface), COMPAT_MIDI_SETTING), 		\
	LISTIFY_ON_COMPAT_IN_SETTING, F, compat, dev, iface, __VA_ARGS__)

/* Perform UTIL_LISTIFY() on all downstream compatibles of a device*/
#define	LISTIFY_ON_COMPAT_IN_DEVICE(F, compat, dev, ...)		\
	UTIL_LISTIFY_LEVEL_1(COMPAT_COUNT(			\
		DEV_N_ID(dev), compat), 				\
		F, dev, __VA_ARGS__)									\
	UTIL_LISTIFY_LEVEL_1(COMPAT_COUNT(DEV_N_ID(dev), COMPAT_MIDI_INTERFACE), 		\
			LISTIFY_ON_COMPAT_IN_INTERFACE, F, compat, dev, __VA_ARGS__)

/* Check whether property of node is set to a value. returns 1 if it is. */
#define	PROP_VAL_IS(node_id, prop, val) 				\
	IS_N(val, DT_ENUM_IDX(node_id, prop))


#define PROP_VAL_LIST(idx, node_id, phs, prop) \
	DT_PROP_BY_PHANDLE_IDX(node_id, phs, idx, prop),

#define LISTIFY_PHANDLES_PROP(node_id, phs, prop) \
	LIST_DROP_EMPTY(UTIL_LISTIFY_LEVEL_4(DT_PROP_LEN(node_id, phs), PROP_VAL_LIST, node_id, phs, prop))


enum jack_type {
	EMBEDDED,
	EXTERNAL
};

struct usb_midi_jack_config {
	uint8_t id;
	enum jack_type type;
};

struct usb_midi_jack_pair_config {
	uint8_t id_in;
	uint8_t id_out;
	enum jack_type type_in;
	enum jack_type type_out;
	// struct usb_midi_jack_config in;
	// struct usb_midi_jack_config out;
};


/**
 * @warning Size of baInterface is 16 just to make it useable
 * for all kind of devices. Actual size of the struct should
 *  be checked by reading .bLength.
 */
struct cs_ac_if_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint16_t bcdADC;
	uint16_t wTotalLength;
	uint8_t bInCollection;
	uint8_t baInterfaceNr[16];
} __packed;

struct std_ms_if_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubclass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} __packed;

struct cs_ms_if_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint16_t bcdMSC;
	uint16_t wTotalLength;
} __packed;

struct cs_midi_in_jack_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bJackType;
	uint8_t bJackID;
	uint8_t iJack;
} __packed;

struct cs_midi_out_jack_descriptor {					
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bJackType;
	uint8_t bJackID;
	uint8_t bNrInputPins;
	uint8_t baSourceID;
	uint8_t BaSourcePin;
	uint8_t	iJack;
} __packed;

struct cs_group_terminal_block_header_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint16_t wTotalLength;
} __packed;

struct cs_group_terminal_block_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bGrpTrmBlkID;
	uint8_t bGrpTrmBlkType;
	uint8_t nGroupTrm;
	uint8_t nNumGroupTrm;
	uint8_t iBlockItem;
	uint8_t bMIDIProtocol;
	uint16_t wMaxInputBandwidth;
	uint16_t wMaxOutputBandwidth;
} __packed;

struct std_ms_1_0_endpoint_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
	uint8_t bRefresh;
	uint8_t bSynchAddress;
} __packed;

struct std_ms_2_0_endpoint_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} __packed;



#define DECLARE_HEADER(dev)		\
struct cs_ac_if_descriptor_##dev {	\
	uint8_t bLength;				\
	uint8_t bDescriptorType;		\
	uint8_t bDescriptorSubtype;		\
	uint16_t bcdADC;				\
	uint16_t wTotalLength;			\
	uint8_t bInCollection;			\
	uint8_t baInterfaceNr[COMPAT_COUNT(DEV_N_ID(dev), COMPAT_MIDI_INTERFACE)];	\
} __packed;

#define DECLARE_CS_ENDPOINT_DESCRIPTOR(endpoint, dev, iface, set, _)	\
struct cs_endpoint_descriptor_##dev##iface##set##endpoint {					\
	uint8_t bLength;									\
	uint8_t bDescriptorType;							\
	uint8_t bDescriptorSubtype;							\
	uint8_t bNumAssoc;									\
	uint8_t bAssoc[DT_PROP_LEN(ENDPOINT_N_ID(dev, iface, set, endpoint), \
		assoc_entities)];									\
} __packed;

#define INIT_IAD(iface_subclass, if_cnt)			\
{								\
	.bLength = sizeof(struct usb_association_descriptor),	\
	.bDescriptorType = USB_ASSOCIATION_DESC,		\
	.bFirstInterface = 0,					\
	.bInterfaceCount = if_cnt,				\
	.bFunctionClass = AUDIO_CLASS,				\
	.bFunctionSubClass = iface_subclass,			\
	.bFunctionProtocol = 0,					\
	.iFunction = 0,						\
}

#define MIDI_JACK_PAIR_TOTAL_SIZE(dev, iface, set)	\
	COMPAT_COUNT(SET_N_ID(dev, iface, set), COMPAT_MIDI_JACK_PAIR) *	\
	(sizeof(struct cs_midi_in_jack_descriptor) + \
	sizeof(struct cs_midi_out_jack_descriptor))


#define MIDI_EP_SIZE(endpoint, dev, iface, set, _)	\
	sizeof(struct cs_endpoint_descriptor_##dev##iface##set##endpoint) +


#define GT_BLOCK_TOTAL_SIZE(dev, iface, set) \
	sizeof(struct cs_group_terminal_block_header_descriptor) +	\
	COMPAT_COUNT(SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK) * 	\
	sizeof(struct cs_group_terminal_block_descriptor)

#define MIDI_EP_TOTAL_SIZE(dev, iface, set)	\
	UTIL_COND_CHOICE_N(2,	\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_1_0), \
		PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_2_0), \
		(sizeof(struct std_ms_1_0_endpoint_descriptor)), \
		(sizeof(struct std_ms_2_0_endpoint_descriptor))) *	\
	COMPAT_COUNT(SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT) +	\
	LISTIFY_ON_COMPAT_IN_SETTING(set, MIDI_EP_SIZE, COMPAT_MIDI_ENDPOINT, dev, iface) \

#define USB_CS_IFACE_DESC_TOTAL_SIZE(dev, iface, set)	\
	COND_NODE_HAS_COMPAT_CHILD(SET_N_ID(dev, iface, set), COMPAT_MIDI_JACK_PAIR, \
	(MIDI_JACK_PAIR_TOTAL_SIZE(dev, iface, set)), (0)) + \
	COND_NODE_HAS_COMPAT_CHILD(SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT, \
	(MIDI_EP_TOTAL_SIZE(dev, iface, set)), (0))  \
	COND_NODE_HAS_COMPAT_CHILD(SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK, \
	(GT_BLOCK_TOTAL_SIZE(dev, iface, set)), (0)) +	\
	sizeof(struct cs_ms_if_descriptor)

#ifdef CONFIG_USB_COMPOSITE_DEVICE
#define USB_MIDI_IAD_DECLARE struct usb_association_descriptor iad;
#define USB_MIDI_IAD(if_cnt)  .iad = INIT_IAD(USB_AUDIO_AUDIOCONTROL, if_cnt),
#else
#define USB_MIDI_IAD_DECLARE
#define USB_MIDI_IAD(if_cnt)
#endif

/* Declare an in jack descriptor */
#define IN_OUT_JACK_DESCRIPTOR(jack, iface, set) 	\
	struct cs_midi_in_jack_descriptor	in_jack_##iface##set##jack; \
	struct cs_midi_out_jack_descriptor	out_jack_##iface##set##jack;

#define MIDI_1_0_ENDPOINT_DESCRIPTOR(endpoint, dev, iface, set)					\
	struct std_ms_1_0_endpoint_descriptor std_ep_##iface##set##endpoint;	\
	struct cs_endpoint_descriptor_##dev##iface##set##endpoint cs_ep_##iface##set##endpoint;

/* Declare a MIDI 1.0 alternate setting descriptor */
#define	MIDI_1_0_SETTING_DESCRIPTOR(dev, iface, set) 			\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_JACK_PAIR), 	\
		IN_OUT_JACK_DESCRIPTOR, iface, set)					\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT), 	\
		MIDI_1_0_ENDPOINT_DESCRIPTOR, dev, iface, set)

#define MIDI_2_0_ENDPOINT_DESCRIPTOR(endpoint, dev, iface, set)								\
	struct std_ms_2_0_endpoint_descriptor std_ep_##iface##set##endpoint;	\
	struct cs_endpoint_descriptor_##dev##iface##set##endpoint cs_ep_##iface##set##endpoint;

#define GROUP_TERMINAL_BLOCK_DESCRIPTOR(gt_block, iface, set)		\
	struct cs_group_terminal_block_descriptor										\
		group_terminal_block_##iface##set##gt_block;

/* Declare a MIDI 2.0 alternate setting descriptor */
#define	MIDI_2_0_SETTING_DESCRIPTOR(dev, iface, set) 			\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT), 	\
		MIDI_2_0_ENDPOINT_DESCRIPTOR, dev, iface, set)						\
	COND_CODE_0(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK),		\
		(), (struct cs_group_terminal_block_header_descriptor 			\
					group_terminal_block_header;))						\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK), 	\
		GROUP_TERMINAL_BLOCK_DESCRIPTOR, iface, set)

/* Declare an alternate setting descriptor */
#define MIDI_SETTING_DESCRIPTOR(set, dev, iface) 			\
	struct	std_ms_if_descriptor std_ms_interface_##iface##set;	\
	struct cs_ms_if_descriptor cs_ms_interface_##iface##set;\
	UTIL_COND_CHOICE_N(2,	\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_1_0), \
		PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_2_0), \
		(MIDI_1_0_SETTING_DESCRIPTOR(dev, iface, set)), \
		(MIDI_2_0_SETTING_DESCRIPTOR(dev, iface, set)))

/* Declare a MIDI interface descriptor */
#define	MIDI_INTERFACE_DESCRIPTOR(iface, dev)		\
	UTIL_LISTIFY_LEVEL_2(COMPAT_COUNT(IFACE_N_ID(dev, iface), COMPAT_MIDI_SETTING), 			\
		MIDI_SETTING_DESCRIPTOR, dev, iface)

/* Declare a MIDI device descriptor */
#define DECLARE_MIDI_DEVICE_DESCRIPTOR(dev) 			\
	struct midi_device_descriptor_##dev {				\
		USB_MIDI_IAD_DECLARE							\
		struct usb_if_descriptor std_ac_interface; 		\
		struct cs_ac_if_descriptor_##dev cs_ac_interface;		\
		UTIL_LISTIFY_LEVEL_1(COMPAT_COUNT(DEV_N_ID(dev), COMPAT_MIDI_INTERFACE), 	\
			MIDI_INTERFACE_DESCRIPTOR, dev)				\
}__packed;

#define DECLARE_MIDI_FUNCTION(dev) 										\
	LISTIFY_ON_COMPAT_IN_DEVICE(										\
		DECLARE_CS_ENDPOINT_DESCRIPTOR, COMPAT_MIDI_ENDPOINT, dev)		\
	DECLARE_HEADER(dev)													\
	DECLARE_MIDI_DEVICE_DESCRIPTOR(dev)

#define INIT_STD_AC_IF	\
{									\
	.bLength = sizeof(struct usb_if_descriptor),			\
	.bDescriptorType = USB_DESC_INTERFACE,				\
	.bInterfaceNumber = 0x00,					\
	.bAlternateSetting = 0x00,				\
	.bNumEndpoints = 0,					\
	.bInterfaceClass = USB_BCC_AUDIO,					\
	.bInterfaceSubClass = USB_AUDIO_AUDIOCONTROL,				\
	.bInterfaceProtocol = 0,					\
	.iInterface = 0						\
}

#define LIST_IFACES(iface, dev, _)	\
	DT_PROP(IFACE_N_ID(dev, iface), index) ,

#define INIT_CS_AC_IF(dev)					\
{									\
	.bLength = sizeof(struct cs_ac_if_descriptor_##dev),	\
	.bDescriptorType = USB_DESC_CS_INTERFACE,			\
	.bDescriptorSubtype = USB_AUDIO_HEADER,				\
	.bcdADC = sys_cpu_to_le16(0x0100),				\
	.wTotalLength = sys_cpu_to_le16(0x0009),				\
	.bInCollection = COMPAT_COUNT(	\
		DEV_N_ID(dev), COMPAT_MIDI_INTERFACE),					\
	.baInterfaceNr = {LIST_DROP_EMPTY(	\
		LISTIFY_ON_COMPAT_IN_DEVICE(	\
			LIST_IFACES, COMPAT_MIDI_INTERFACE, dev))}						\
}

#define INIT_STD_MS_IF(dev, iface, set)				\
{															\
	.bLength = sizeof(struct std_ms_if_descriptor),			\
	.bDescriptorType = USB_DESC_INTERFACE,					\
	.bInterfaceNumber = DT_PROP(IFACE_N_ID(dev, iface), index),							\
	.bAlternateSetting = DT_PROP(SET_N_ID(dev, iface, set), index),							\
	.bNumEndpoints = COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT),	\
	.bInterfaceClass = USB_BCC_AUDIO,									\
	.bInterfaceSubclass = USB_AUDIO_MIDISTREAMING,					\
	.bInterfaceProtocol = 0x00,										\
	.iInterface = 0x00												\
}

#define INIT_CS_MS_IF(dev, iface, set)		\
{											\
	.bLength = sizeof(struct cs_ms_if_descriptor),	\
	.bDescriptorType = USB_DESC_CS_INTERFACE,		\
	.bDescriptorSubtype = USB_MS_HEADER,			\
	.bcdMSC = UTIL_COND_CHOICE_N(2,					\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), 		\
			revision, SETTING_MIDI_1_0), 			\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), 		\
			revision, SETTING_MIDI_2_0), 			\
		(sys_cpu_to_le16(0x0100)), 					\
		(sys_cpu_to_le16(0x0200))),					\
	.wTotalLength = UTIL_COND_CHOICE_N(2,			\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), 		\
			revision, SETTING_MIDI_1_0), 			\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), 		\
			revision, SETTING_MIDI_2_0), 			\
		(sys_cpu_to_le16(USB_CS_IFACE_DESC_TOTAL_SIZE(dev, iface, set))), 	\
		(sys_cpu_to_le16(sizeof(struct cs_ms_if_descriptor)))),				\
}

#define INIT_CS_MIDI_IN_JACK(dev, iface, set, jack)	\
{							\
	.bLength = sizeof(struct cs_midi_in_jack_descriptor),		\
	.bDescriptorType = USB_DESC_CS_INTERFACE,	\
	.bDescriptorSubtype = USB_MIDI_IN_JACK,\
	.bJackType = UTIL_COND_CHOICE_N(2,	\
		PROP_VAL_IS(JACK_PAIR_N_ID(dev, iface, set, jack), 	\
			type_in, JACK_EMBEDDED), 					\
		PROP_VAL_IS(JACK_PAIR_N_ID(dev, iface, set, jack), 	\
			type_in, JACK_EXTERNAL), 					\
		(0x01), (0x02)),			\
	.bJackID = DT_PROP(JACK_PAIR_N_ID(dev, iface, set, jack), id_in),	\
	.iJack = 0x00,		\
}

#define INIT_CS_MIDI_OUT_JACK(dev, iface, set, jack)	\
{							\
	.bLength = sizeof(struct cs_midi_out_jack_descriptor),		\
	.bDescriptorType = USB_DESC_CS_INTERFACE,	\
	.bDescriptorSubtype = USB_MIDI_OUT_JACK,\
	.bJackType = UTIL_COND_CHOICE_2(	\
		PROP_VAL_IS(JACK_PAIR_N_ID(dev, iface, set, jack), 	\
			type_out, JACK_EMBEDDED), 					\
		PROP_VAL_IS(JACK_PAIR_N_ID(dev, iface, set, jack), 	\
			type_out, JACK_EXTERNAL), 					\
		(0x01), (0x02)),			\
	.bJackID = DT_PROP(JACK_PAIR_N_ID(dev, iface, set, jack), id_out),	\
	.bNrInputPins = 0x01,	\
	.baSourceID = DT_PROP(JACK_PAIR_N_ID(dev, iface, set, jack), id_in), 		\
	.BaSourcePin = DT_PROP(JACK_PAIR_N_ID(dev, iface, set, jack), id_in), \
	.iJack = 0x00,	\
}

/** refer to Table 4-7 from audio10.pdf
 */
#define INIT_CS_GT_BLK_HEADER(dev, iface, set)			\
{									\
	.bLength = sizeof(struct cs_group_terminal_block_header_descriptor),	\
	.bDescriptorType = USB_CS_GR_TRM_BLOCK,			\
	.bDescriptorSubtype = USB_GR_TRM_BLOCK_HEADER,			\
	.wTotalLength = GT_BLOCK_TOTAL_SIZE(dev, iface, set)			\
}

/* Class-Specific AS Interface Descriptor 4.5.2 audio10.pdf */
#define INIT_CS_GT_BLK(dev, iface, set, gt_blk)			\
{																	\
	.bLength = sizeof(struct cs_group_terminal_block_descriptor),	\
	.bDescriptorType = USB_CS_GR_TRM_BLOCK,	\
	.bDescriptorSubtype = USB_GR_TRM_BLOCK,	\
	.bGrpTrmBlkID = DT_PROP(GT_BLK_N_ID(dev, iface, set, gt_blk), id),				\
	.bGrpTrmBlkType = UTIL_COND_CHOICE_N(3,	\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk),  	\
			type, GT_BLK_IN), 								\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk),  	\
			type, GT_BLK_OUT),								\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk),  	\
			type, GT_BLK_IN_OUT),							\
		(0x01), (0x02), (0x00)),	\
	.nGroupTrm = DT_PROP(	\
		GT_BLK_N_ID(dev, iface, set, gt_blk),\
			group_terminal),		\
	.nNumGroupTrm = DT_PROP(	\
		GT_BLK_N_ID(dev, iface, set, gt_blk),\
			num_group_terminals),	\
	.iBlockItem = 0x00,		\
	.bMIDIProtocol = UTIL_COND_CHOICE_N(7,	\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk), 	\
			protocol, GT_BLK_UNKNOWN), 							\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk), 	\
			protocol, GT_BLK_MIDI_1_0_64),						\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk),	\
		 	protocol, GT_BLK_MIDI_1_0_64_TIMESTAMPS),			\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk),	\
		 	protocol, GT_BLK_MIDI_1_0_128),						\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk),	\
		 	protocol, GT_BLK_MIDI_1_0_128_TIMESTAMPS),			\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk),	\
		 	protocol, GT_BLK_MIDI_2_0),							\
		PROP_VAL_IS(GT_BLK_N_ID(dev, iface, set, gt_blk),	\
		 	protocol, GT_BLK_MIDI_2_0_TIMESTAMPS),				\
		(0x00), (0x01), (0x02), (0x03), (0x04), (0x11), (0x12)),	\
	.wMaxInputBandwidth = DT_PROP(	\
		GT_BLK_N_ID(dev, iface, set, gt_blk),\
			in_bandwidth),	\
	.wMaxOutputBandwidth = DT_PROP(	\
		GT_BLK_N_ID(dev, iface, set, gt_blk),\
			out_bandwidth)	\
}

#define INIT_STD_MS_MIDI_1_0_EP(dev, iface, set, endpoint)			\
{																	\
	.bLength = sizeof(struct std_ms_1_0_endpoint_descriptor),		\
	.bDescriptorType = USB_DESC_ENDPOINT,							\
	.bEndpointAddress = (											\
		PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), 		\
			direction, ENDPOINT_IN) << 7) |	0x00,					\
	.bmAttributes = 0x02,											\
	.wMaxPacketSize = sys_cpu_to_le16(0x0040),						\
	.bInterval = 0x00,												\
	.bRefresh = 0x00,												\
	.bSynchAddress = 0x00											\
}

#define INIT_STD_MS_MIDI_2_0_EP(dev, iface, set, endpoint)					\
{									\
	.bLength = sizeof(struct std_ms_2_0_endpoint_descriptor),	\
	.bDescriptorType = USB_DESC_ENDPOINT,				\
	.bEndpointAddress = (	\
		PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), \
			direction, ENDPOINT_IN) << 7) | 0x00,					\
	.bmAttributes = 0x02 |\
	 	(PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), \
			type, ENDPOINT_INTERRUPT)),	\
	.wMaxPacketSize = sys_cpu_to_le16(0x0040),		\
	.bInterval = COND_CODE_1(PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), \
			type, ENDPOINT_BULK), (0x00), (DT_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), interval)))						\
}

#define INIT_MIDI_1_0_CS_MS_EP(dev, iface, set, endpoint)						\
{								\
	.bLength = sizeof(struct cs_endpoint_descriptor_##dev##iface##set##endpoint),	\
	.bDescriptorType = USB_DESC_CS_ENDPOINT,		\
	.bDescriptorSubtype = USB_MS_GENERAL,				\
	.bNumAssoc = DT_PROP_LEN(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities), \
	UTIL_COND_CHOICE_N(2, \
		PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), direction, ENDPOINT_IN),\
		PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), direction, ENDPOINT_OUT),\
		(.bAssoc = {LISTIFY_PHANDLES_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities, id_out)}), 	\
		(.bAssoc = {LISTIFY_PHANDLES_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities, id_in)}))	\
}

#define INIT_MIDI_2_0_CS_MS_EP(dev, iface, set, endpoint)						\
{								\
	.bLength = sizeof(struct cs_endpoint_descriptor_##dev##iface##set##endpoint),	\
	.bDescriptorType = USB_DESC_CS_ENDPOINT,		\
	.bDescriptorSubtype = USB_MS_GENERAL_2_0,				\
	.bNumAssoc =  DT_PROP_LEN(ENDPOINT_N_ID(dev, iface, set, endpoint), \
		assoc_entities),					\
	.bAssoc = {LISTIFY_PHANDLES_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities, id)},				\
}

#define INIT_EP_DATA(cb, addr)	\
	{			\
		.ep_cb = cb,	\
		.ep_addr = addr,\
	}

#define	DEFINE_IN_OUT_JACK_DESCRIPTOR(jack, dev, iface, set) \
	.in_jack_##iface##set##jack = INIT_CS_MIDI_IN_JACK(dev, iface, set, jack), \
	.out_jack_##iface##set##jack = INIT_CS_MIDI_OUT_JACK(dev, iface, set, jack),\
	

#define DEFINE_MIDI_1_0_ENDPOINT_DESCRIPTOR(endpoint, dev, iface, set) \
	.std_ep_##iface##set##endpoint = INIT_STD_MS_MIDI_1_0_EP(dev, iface, set, endpoint), \
	.cs_ep_##iface##set##endpoint = INIT_MIDI_1_0_CS_MS_EP(dev, iface, set, endpoint),\

#define DEFINE_MIDI_1_0_SETTING_DESCRIPTOR(dev, iface, set) \
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_JACK_PAIR), 	\
		DEFINE_IN_OUT_JACK_DESCRIPTOR, dev, iface, set)					\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_JACK_PAIR), 	\
		DEFINE_IN_OUT_JACK_DESCRIPTOR, dev, iface, set)					\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT), 	\
		DEFINE_MIDI_1_0_ENDPOINT_DESCRIPTOR, dev, iface, set)

#define DEFINE_MIDI_2_0_ENDPOINT_DESCRIPTOR(endpoint, dev, iface, set)	\
	.std_ep_##iface##set##endpoint = INIT_STD_MS_MIDI_2_0_EP(dev, iface, set, endpoint), \
	.cs_ep_##iface##set##endpoint =  INIT_MIDI_2_0_CS_MS_EP(dev, iface, set, endpoint), \

#define DEFINE_GROUP_TERMINAL_BLOCK_HEADER_DESCRIPTOR(dev, iface, set)		\
	.group_terminal_block_header = INIT_CS_GT_BLK_HEADER(dev, iface, set),

#define DEFINE_GROUP_TERMINAL_BLOCK_DESCRIPTOR(gt_block, dev, iface, set)		\
	.group_terminal_block_##iface##set##gt_block = INIT_CS_GT_BLK(dev, iface, set, gt_blk), \

#define DEFINE_MIDI_2_0_SETTING_DESCRIPTOR(dev, iface, set) \
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT), 	\
		DEFINE_MIDI_2_0_ENDPOINT_DESCRIPTOR, dev, iface, set)						\
	COND_CODE_0(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK),		\
		(), (DEFINE_GROUP_TERMINAL_BLOCK_HEADER_DESCRIPTOR(dev, iface, set)))						\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK), 	\
		DEFINE_GROUP_TERMINAL_BLOCK_DESCRIPTOR, dev, iface, set)

#define DEFINE_MIDI_SETTING_DESCRIPTOR(set, dev, iface)	\
	.std_ms_interface_##iface##set = INIT_STD_MS_IF(dev, iface, set),	 \
	.cs_ms_interface_##iface##set = INIT_CS_MS_IF(dev, iface, set), \
	UTIL_COND_CHOICE_N(2,	\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_1_0), \
		PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_2_0), \
		(DEFINE_MIDI_1_0_SETTING_DESCRIPTOR(dev, iface, set)), \
		(DEFINE_MIDI_2_0_SETTING_DESCRIPTOR(dev, iface, set)))

#define DEFINE_MIDI_INTERFACE_DESCRIPTOR(iface, dev)	\
	UTIL_LISTIFY_LEVEL_2(COMPAT_COUNT(IFACE_N_ID(dev, iface), COMPAT_MIDI_SETTING), 			\
		DEFINE_MIDI_SETTING_DESCRIPTOR, dev, iface)

#define DEFINE_MIDI_DESCRIPTOR(dev)	\
	USBD_CLASS_DESCR_DEFINE(primary, midi)	\
	struct midi_device_descriptor_##dev midi_device_##dev = { \
		USB_MIDI_IAD(COMPAT_COUNT(DEV_N_ID(dev), COMPAT_MIDI_INTERFACE) + 1)	\
		.std_ac_interface = INIT_STD_AC_IF, \
		.cs_ac_interface = INIT_CS_AC_IF(dev),\
		UTIL_LISTIFY_LEVEL_1(COMPAT_COUNT(DEV_N_ID(dev), COMPAT_MIDI_INTERFACE), 	\
			DEFINE_MIDI_INTERFACE_DESCRIPTOR, dev)	 \
	};

#endif /* ZEPHYR_INCLUDE_USB_CLASS_MIDI_INTERNAL_H_ */

