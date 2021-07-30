
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

/* Names of compatibles used for configuration of the device */
#define COMPAT_MIDI_DEVICE  				usb_midi_device
#define COMPAT_MIDI_INTERFACE				usb_midi_interface
#define COMPAT_MIDI_SETTING  				usb_midi_setting
#define COMPAT_MIDI_IN_JACK  				usb_midi_in_jack
#define COMPAT_MIDI_OUT_JACK  				usb_midi_out_jack
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

#define ENDPOINT_OUT	0
#define ENDPOINT_IN		1

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
#define COMPAT_COUNT(node_id, compat)	\
	COND_CODE_1(DT_NODE_EXISTS(GET_ARG_N(1, COMPAT_CHILDREN_LIST(node_id, compat))),	\
	 (NUM_VA_ARGS_LESS_1(COMPAT_CHILDREN_LIST(node_id, compat),)),(0))

/* Get MIDI device node ID */
#define DEV_N_ID(dev)	DT_INST(dev, COMPAT_MIDI_DEVICE)

/* Get MIDI setting node ID */
#define IFACE_N_ID(dev, iface)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(iface), COMPAT_CHILDREN_LIST(		\
		DEV_N_ID(dev), COMPAT_MIDI_INTERFACE))

/* Get MIDI interface node ID */
#define SET_N_ID(dev, iface, set)						\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(set), COMPAT_CHILDREN_LIST( 	\
	IFACE_N_ID(dev, iface), COMPAT_MIDI_SETTING))

/* Get MIDI out jack node ID */
#define OUT_JACK_N_ID(dev, iface, set, jack)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(jack), COMPAT_CHILDREN_LIST(		\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_OUT_JACK))

/* Get MIDI in jack node ID */
#define IN_JACK_N_ID(dev, iface, set, jack)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(jack), COMPAT_CHILDREN_LIST(		\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_IN_JACK))

/* Get MIDI in jack node ID */
#define GT_BLK_N_ID(dev, iface, set, gt_blk)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(gt_blk), COMPAT_CHILDREN_LIST(		\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK))

/* Get MIDI in jack node ID */
#define ENDPOINT_N_ID(dev, iface, set, endpoint)				\
	GET_ARG_N_EXPANDED(INCREMENT_NUM_1(endpoint), COMPAT_CHILDREN_LIST(		\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT))

/* Number of MIDI devices*/
#define MIDI_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI) 

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

/* Check whether an interface is MIDI 1.0. Returns 1 if it is. */
#define	PROP_VAL_IS(node_id, prop, val) 				\
	IS_N(val, DT_ENUM_IDX(node_id, prop))


#define PROP_VAL_LIST(idx, node_id, phs, prop) \
	DT_PROP_BY_PHANDLE_IDX(node_id, phs, idx, prop),

#define LISTIFY_PHANDLES_PROP(node_id, phs, prop) \
	LIST_DROP_EMPTY(UTIL_LISTIFY(DT_PROP_LEN(node_id, phs), PROP_VAL_LIST, node_id, phs, prop))




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
	uint16_t bGrpTrmBlkType;
	uint8_t nGroupTrm;
	uint8_t nNumGroupTrm;
	uint8_t iBlockItem;
	uint8_t bMIDIProtocol;
	uint8_t wMaxInputBandwidth;
	uint8_t wMaxOutputBandwidth;
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

#define OUT_JACK_PIN(pin, _) 	\
	uint8_t baSourceID ## pin; 	\
	uint8_t BaSourcePin ## pin;

#define DECLARE_MIDI_OUT_JACK_DESCRIPTOR(jack, dev, iface, set)	\
struct cs_midi_out_jack_descriptor_##dev_##iface####_##set##_##jack {					\
	uint8_t bLength;									\
	uint8_t bDescriptorType;							\
	uint8_t bDescriptorSubtype;							\
	uint8_t bJackType;									\
	uint8_t bJackID;									\
	uint8_t bNrInputPins;								\
	UTIL_LISTIFY_LEVEL_4(DT_PROP_LEN(OUT_JACK_N_ID(dev, iface, set, jack), \
		sources), OUT_JACK_PIN)	\
	uint8_t	iJack;\
} __packed;

#define DECLARE_CS_ENDPOINT_DESCRIPTOR(endpoint, dev, iface, set)	\
struct cs_endpoint_descriptor_##dev_##iface####_##set##_##endpoint {					\
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

#ifdef CONFIG_USB_COMPOSITE_DEVICE
#define USB_MIDI_IAD_DECLARE struct usb_association_descriptor iad;
#define USB_MIDI_IAD(if_cnt)  .iad = INIT_IAD(USB_AUDIO_AUDIOCONTROL, if_cnt),
#else
#define USB_MIDI_IAD_DECLARE
#define USB_MIDI_IAD(if_cnt)
#endif


/* Declare an in jack descriptor */
#define IN_JACK_DESCRIPTOR(jack, iface, set) 	\
	struct cs_midi_in_jack_descriptor	in_jack_##iface##_##set##_##jack;

/* Declare an out jack descriptor */
#define OUT_JACK_DESCRIPTOR(jack, dev, iface, set) 						\
	struct cs_midi_out_jack_descriptor_##dev##_##iface##_##set##_##jack	\
		out_jack_##iface##_##set##_##jack;

#define MIDI_1_0_ENDPOINT_DESCRIPTOR(endpoint, iface, set)								\
	struct std_ms_1_0_endpoint_descriptor std_ep_##iface##_##set##_##endpoint;	\
	struct cs_as_ad_ep_descriptor cs_ep_##iface##_##set##_##endpoint;

/* Declare a MIDI 1.0 alternate setting descriptor */
#define	MIDI_1_0_SETTING_DESCRIPTOR(dev, iface, set) 			\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_IN_JACK), 	\
		IN_JACK_DESCRIPTOR, iface, set)					\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_OUT_JACK), 	\
		OUT_JACK_DESCRIPTOR, dev, iface, set)					\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT), 	\
		MIDI_1_0_ENDPOINT_DESCRIPTOR, iface, set)

#define MIDI_2_0_ENDPOINT_DESCRIPTOR(endpoint, iface, set)								\
	struct std_ms__0_endpoint_descriptor std_ep_##iface##_##set##_##endpoint;	\
	struct cs_as_ad_ep_descriptor cs_ep_##iface##_##set##_##endpoint;

#define GROUP_TERMINAL_BLOCK_DESCRIPTOR(gt_block, iface, set)		\
	struct cs_group_terminal_block_descriptor										\
		group_terminal_block_##iface##_##set##_##gt_block;

/* Declare a MIDI 2.0 alternate setting descriptor */
#define	MIDI_2_0_SETTING_DESCRIPTOR(dev, iface, set) 			\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT), 	\
		MIDI_2_0_ENDPOINT_DESCRIPTOR, iface, set)						\
	COND_CODE_0(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK),		\
		(), (struct cs_group_terminal_block_header_descriptor 			\
					group_terminal_block_header;))						\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK), 	\
		GROUP_TERMINAL_BLOCK_DESCRIPTOR, iface, set)

/* Declare an alternate setting descriptor */
#define MIDI_SETTING_DESCRIPTOR(set, dev, iface) 			\
	struct	std_ms_if_descriptor std_ms_interface_##iface##_##set;	\
	struct cs_ms_if_descriptor cs_ms_interface_##iface##_##set;\
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
		DECLARE_MIDI_OUT_JACK_DESCRIPTOR, COMPAT_MIDI_OUT_JACK, dev)	\
	LISTIFY_ON_COMPAT_IN_DEVICE(										\
		DECLARE_CS_ENDPOINT_DESCRIPTOR, COMPAT_MIDI_ENDPOINT, dev)		\
	DECLARE_HEADER(dev)													\
	DECLARE_MIDI_DEVICE_DESCRIPTOR(dev)

#define INIT_STD_AC_IF	\
{									\
	.bLength = sizeof(struct usb_if_descriptor),			\
	.bDescriptorType = USB_INTERFACE_DESC,				\
	.bInterfaceNumber = 0x00,					\
	.bAlternateSetting = 0x00,				\
	.bNumEndpoints = 0,					\
	.bInterfaceClass = AUDIO_CLASS,					\
	.bInterfaceSubClass = USB_AUDIO_AUDIOCONTROL,				\
	.bInterfaceProtocol = 0,					\
	.iInterface = 0						\
}

#define LIST_IFACES(iface, dev)	\
	DT_PROP(IFACE_N_ID(dev, iface), index) ,

#define INIT_CS_AC_IF(dev)					\
{									\
	.bLength = sizeof(struct cs_ac_if_descriptor),	\
	.bDescriptorType = USB_CS_INTERFACE_DESC,			\
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
	.bDescriptorType = USB_INTERFACE_DESC,					\
	.bInterfaceNumber = DT_PROP(IFACE_N_ID(dev, iface), index),							\
	.bAlternateSetting = DT_PROP(SET_N_ID(dev, iface, set), index),							\
	.bNumEndpoints = COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT),	\
	.bInterfaceClass = AUDIO_CLASS,									\
	.bInterfaceSubclass = USB_AUDIO_MIDISTREAMING,					\
	.bInterfaceProtocol = 0x00,										\
	.iInterface = 0x00												\
}

#define INIT_CS_MS_IF(dev, iface, set)		\
{											\
	.bLength = sizeof(struct cs_ms_if_descriptor),			\
	.bDescriptorType = USB_CS_INTERFACE_DESC,		\
	.bDescriptorSubtype = USB_MS_HEADER,			\
	.bcdMSC = UTIL_COND_CHOICE_N(2,					\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), 		\
			revision, SETTING_MIDI_1_0), 			\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), 		\
			revision, SETTING_MIDI_2_0), 			\
		(sys_cpu_to_le16(0x0100)), 					\
		(sys_cpu_to_le16(0x0200))),					\
	.wTotalLength = 0x00							\
}

#define INIT_CS_MIDI_IN_JACK(dev, iface, set, jack)	\
{							\
	.bLength = sizeof(struct cs_midi_in_jack_descriptor),		\
	.bDescriptorType = USB_CS_INTERFACE_DESC,	\
	.bDescriptorSubtype = USB_MIDI_IN_JACK,\
	.bJackType = UTIL_COND_CHOICE_2(	\
		PROP_VAL_IS(IN_JACK_N_ID(dev, iface, set, jack), 	\
			type, JACK_EMBEDDED), 					\
		PROP_VAL_IS(IN_JACK_N_ID(dev, iface, set, jack), 	\
			type, JACK_EXTERNAL), 					\
		(0x01), (0x02)),			\
	.bJackID = DT_PROP(IN_JACK_N_ID(dev, iface, set, jack), id),	\
	.iJack = 0x00		\
}

#define INIT_OUT_JACK_PIN(pin, dev, iface, set, jack)	\
	.baSourceID##pin = DT_PROP_BY_PHANDLE_IDX(	\
		OUT_JACK_N_ID(dev, iface, set, jack), sources, pin, id), 		\
	.BaSourcePin##pin = DT_PROP_BY_IDX(	\
		OUT_JACK_N_ID(dev, iface, set, jack), sources_pins, pin),

#define INIT_CS_MIDI_OUT_JACK(dev, iface, set, jack)	\
{							\
	.bLength = sizeof(struct cs_midi_out_jack_descriptor_##dev##_##iface####_##set##_##jack),		\
	.bDescriptorType = USB_CS_INTERFACE_DESC,	\
	.bDescriptorSubtype = USB_MIDI_OUT_JACK,\
	.bJackType = UTIL_COND_CHOICE_2(	\
		PROP_VAL_IS(OUT_JACK_N_ID(dev, iface, set, jack), 	\
			type, JACK_EMBEDDED), 					\
		PROP_VAL_IS(OUT_JACK_N_ID(dev, iface, set, jack), 	\
			type, JACK_EXTERNAL), 					\
		(0x01), (0x02)),			\
	.bJackID = DT_PROP(OUT_JACK_N_ID(dev, iface, set, jack), id),	\
	.bNrInputPins = DT_PROP_LEN(OUT_JACK_N_ID(dev, iface, set, jack), \
		sources),		\
		UTIL_LISTIFY_LEVEL_4(DT_PROP_LEN(OUT_JACK_N_ID(dev, iface, set, jack), \
		sources), INIT_OUT_JACK_PIN, dev, iface, set, jack)\
	.iJack = 0x00	\
}

/** refer to Table 4-7 from audio10.pdf
 */
#define INIT_CS_GT_BLK_HEADER(dev, iface, set)			\
{									\
	.bLength = sizeof(struct cs_group_terminal_block_header_descriptor),	\
	.bDescriptorType = USB_CS_GR_TRM_BLOCK,			\
	.bDescriptorSubtype = USB_GR_TRM_BLOCK_HEADER,			\
	.wTotalLength = 0x00						\
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
	.bDescriptorType = USB_ENDPOINT_DESC,							\
	.bEndpointAddress = (											\
		PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), 		\
			direction, ENDPOINT_IN) << 7) |							\
		DT_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), ep_num),	\
	.bmAttributes = 0x02,											\
	.wMaxPacketSize = sys_cpu_to_le16(0x0040),						\
	.bInterval = 0x00,												\
	.bRefresh = 0x00,												\
	.bSynchAddress = 0x00											\
}

#define INIT_STD_MS_MIDI_2_0_EP(dev, iface, set, endpoint)					\
{									\
	.bLength = sizeof(struct std_ms_2_0_endpoint_descriptor),	\
	.bDescriptorType = USB_ENDPOINT_DESC,				\
	.bEndpointAddress = (	\
		PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), \
			direction, ENDPOINT_IN) << 7) |\
		DT_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), ep_num),					\
	.bmAttributes = 0x02 |\
	 	(PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), \
			type, ENDPOINT_INTERRUPT) << 7),	\
	.wMaxPacketSize = sys_cpu_to_le16(0x0040),		\
	.bInterval = COND_CODE_1(PROP_VAL_IS(ENDPOINT_N_ID(dev, iface, set, endpoint), \
			type, ENDPOINT_BULK), (0x00), (DT_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), interval)))						\
}

#define INIT_MIDI_1_0_CS_MS_EP(dev, iface, set, endpoint)						\
{								\
	.bLength = sizeof(struct cs_endpoint_descriptor_##dev_##iface####_##set##_##endpoint),	\
	.bDescriptorType = USB_CS_ENDPOINT_DESC,		\
	.bDescriptorSubtype = USB_MS_GENERAL,				\
	.bNumAssoc = DT_PROP_LEN(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities), \
	.bAssoc = {LISTIFY_PHANDLES_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), assoc_entities, id)}				\
}

#define INIT_MIDI_2_0_CS_MS_EP(dev, iface, set, endpoint)						\
{								\
	.bLength = sizeof(struct cs_endpoint_descriptor_##dev_##iface####_##set##_##endpoint),	\
	.bDescriptorType = USB_CS_ENDPOINT_DESC,		\
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

#define	DEFINE_IN_JACK_DESCRIPTOR(jack, dev, iface, set) \
	.in_jack_##iface##_##set##_##jack = INIT_CS_MIDI_IN_JACK(dev, iface, set, jack), \

#define DEFINE_OUT_JACK_DESCRIPTOR(jack, dev, iface, set) \
	.out_jack_##iface##_##set##_##jack = INIT_CS_MIDI_OUT_JACK(dev, iface, set, jack),\

#define DEFINE_MIDI_1_0_ENDPOINT_DESCRIPTOR(endpoint, dev, iface, set) \
	.std_ep_##iface##_##set##_##endpoint = INIT_STD_MS_MIDI_1_0_EP(dev, iface, set, endpoint), \
	.cs_ep_##iface##_##set##_##endpoint = INIT_MIDI_1_0_CS_MS_EP(dev, iface, set, endpoint),\

#define DEFINE_MIDI_1_0_SETTING_DESCRIPTOR(dev, iface, set) \
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_IN_JACK), 	\
		DEFINE_IN_JACK_DESCRIPTOR, dev, iface, set)					\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_OUT_JACK), 	\
		DEFINE_OUT_JACK_DESCRIPTOR, dev, iface, set)					\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT), 	\
		DEFINE_MIDI_1_0_ENDPOINT_DESCRIPTOR, dev, iface, set)

#define DEFINE_MIDI_2_0_ENDPOINT_DESCRIPTOR(endpoint, dev, iface, set)	\
	.std_ep_##iface##_##set##_##endpoint = INIT_STD_MS_MIDI_2_0_EP(dev, iface, set, endpoint), \
	.cs_ep_##iface##_##set##_##endpoint =  INIT_MIDI_2_0_CS_MS_EP(dev, iface, set, endpoint), \

#define DEFINE_GROUP_TERMINAL_BLOCK_DESCRIPTOR(gt_block, dev, iface, set)		\
	.group_terminal_block_##iface##_##set##_##gt_block = INIT_CS_GT_BLK(dev, iface, set, gt_blk), \

#define DEFINE_MIDI_2_0_SETTING_DESCRIPTOR(dev, iface, set) \
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_ENDPOINT), 	\
		DEFINE_MIDI_2_0_ENDPOINT_DESCRIPTOR, dev, iface, set)						\
	COND_CODE_0(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK),		\
		(), (struct cs_group_terminal_block_header_descriptor 			\
					group_terminal_block_header;))						\
	UTIL_LISTIFY_LEVEL_3(COMPAT_COUNT(								\
		SET_N_ID(dev, iface, set), COMPAT_MIDI_GT_BLK), 	\
		DEFINE_GROUP_TERMINAL_BLOCK_DESCRIPTOR, dev, iface, set)

#define DEFINE_MIDI_SETTING_DESCRIPTOR(set, dev, iface)	\
	.std_ms_interface_##iface##_##set = INIT_STD_MS_IF(dev, iface, set),	 \
	.cs_ms_interface_##iface##_##set = INIT_CS_MS_IF(dev, iface, set), \
	UTIL_COND_CHOICE_N(2,	\
		PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_1_0), \
		PROP_VAL_IS(SET_N_ID(dev, iface, set), revision, SETTING_MIDI_2_0), \
		(DEFINE_MIDI_1_0_SETTING_DESCRIPTOR(dev, iface, set)), \
		(DEFINE_MIDI_2_0_SETTING_DESCRIPTOR(dev, iface, set)))

#define DEFINE_MIDI_INTERFACE_DESCRIPTOR(iface, dev)	\
	UTIL_LISTIFY_LEVEL_2(COMPAT_COUNT(IFACE_N_ID(dev, iface), COMPAT_MIDI_SETTING), 			\
		DEFINE_MIDI_SETTING_DESCRIPTOR, dev, iface)

#define ENDPOINT_IN_IS_N(endpoint, dev, iface, set, N) 	\
	COND_CODE_1(PROP_VAL_IS(										\
		ENDPOINT_N_ID(dev, iface, set, endpoint), direction, ENDPOINT_IN),	\
		(COND_CODE_1(	\
			IS_N(	\
				N, DT_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), ep_num)),\
			(1, ), ())), 	\
		())

#define ENDPOINT_IN_N_IS_USED(dev, N) 		\
	 GET_ARG_N(1, LISTIFY_ON_COMPAT_IN_DEVICE(				\
		ENDPOINT_IN_IS_N, COMPAT_MIDI_ENDPOINT, dev, N))

#define ENDPOINT_OUT_IS_N(endpoint, dev, iface, set, N) 	\
	COND_CODE_1(PROP_VAL_IS(										\
		ENDPOINT_N_ID(dev, iface, set, endpoint), direction, ENDPOINT_OUT),	\
		(COND_CODE_1(	\
			IS_N(	\
				N, DT_PROP(ENDPOINT_N_ID(dev, iface, set, endpoint), ep_num)),\
			(1, ), ())), 	\
		())

#define ENDPOINT_OUT_N_IS_USED(dev, N) 		\
	GET_ARG_N(1, LISTIFY_ON_COMPAT_IN_DEVICE(				\
		ENDPOINT_OUT_IS_N, COMPAT_MIDI_ENDPOINT, dev, N))

#define DEFINE_EP_DATA(dev) \
static struct usb_ep_cfg_data usb_midi_ep_data_##dev[] = {		  \
	UTIL_COND_CHOICE_N(8,								\
		ENDPOINT_IN_N_IS_USED(dev, 1),					\
		ENDPOINT_IN_N_IS_USED(dev, 2),					\
		ENDPOINT_IN_N_IS_USED(dev, 3),					\
		ENDPOINT_IN_N_IS_USED(dev, 4),					\
		ENDPOINT_IN_N_IS_USED(dev, 5),					\
		ENDPOINT_IN_N_IS_USED(dev, 6),					\
		ENDPOINT_IN_N_IS_USED(dev, 7),					\
		ENDPOINT_IN_N_IS_USED(dev, 8),					\
		(INIT_EP_DATA(usb_transfer_ep_callback, 0x80)),	\
		(INIT_EP_DATA(usb_transfer_ep_callback, 0x81)),	\
		(INIT_EP_DATA(usb_transfer_ep_callback, 0x82)),	\
		(INIT_EP_DATA(usb_transfer_ep_callback, 0x83)),	\
		(INIT_EP_DATA(usb_transfer_ep_callback, 0x84)),	\
		(INIT_EP_DATA(usb_transfer_ep_callback, 0x85)),	\
		(INIT_EP_DATA(usb_transfer_ep_callback, 0x86)),	\
		(INIT_EP_DATA(usb_transfer_ep_callback, 0x87))),\
	UTIL_COND_CHOICE_N(8,								\
		ENDPOINT_OUT_N_IS_USED(dev, 1),					\
		ENDPOINT_OUT_N_IS_USED(dev, 2),					\
		ENDPOINT_OUT_N_IS_USED(dev, 3),					\
		ENDPOINT_OUT_N_IS_USED(dev, 4),					\
		ENDPOINT_OUT_N_IS_USED(dev, 5),					\
		ENDPOINT_OUT_N_IS_USED(dev, 6),					\
		ENDPOINT_OUT_N_IS_USED(dev, 7),					\
		ENDPOINT_OUT_N_IS_USED(dev, 8),					\
		(INIT_EP_DATA(audio_receive_cb, 0x00)),			\
		(INIT_EP_DATA(audio_receive_cb, 0x01)),			\
		(INIT_EP_DATA(audio_receive_cb, 0x02)),			\
		(INIT_EP_DATA(audio_receive_cb, 0x03)),			\
		(INIT_EP_DATA(audio_receive_cb, 0x04)),			\
		(INIT_EP_DATA(audio_receive_cb, 0x05)),			\
		(INIT_EP_DATA(audio_receive_cb, 0x06)),			\
		(INIT_EP_DATA(audio_receive_cb, 0x07))) 		\
};

#define DEFINE_MIDI_DESCRIPTOR(dev)	\
	USBD_CLASS_DESCR_DEFINE(primary, midi)	\
	struct midi_device_descriptor_##dev midi_device_##dev = { \
		USB_MIDI_IAD(COMPAT_COUNT(DEV_N_ID(dev), COMPAT_MIDI_INTERFACE) + 1)	\
		.std_ac_interface = INIT_STD_AC_IF, \
		.cs_ac_interface = INIT_CS_AC_IF(dev),\
		UTIL_LISTIFY_LEVEL_1(COMPAT_COUNT(DEV_N_ID(dev), COMPAT_MIDI_INTERFACE), 	\
			DEFINE_MIDI_INTERFACE_DESCRIPTOR, dev)	



#define	TEST_STRING	\
	STRINGIFY(\
		(DEFINE_EP_DATA(0))\
	\
	)

#endif /* ZEPHYR_INCLUDE_USB_CLASS_MIDI_INTERNAL_H_ */

