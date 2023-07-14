/**
 * @file
 * @brief SERIAL MIDI internal header
 *
 * This header file is used to store internal configuration
 * defines.
 */
// #include <kernel.h>
// #include <device.h>
// #include "usb/class/usb_midi.h"
#include "sys/util_macro_expansion.h"
#include <zephyr/sys/util_internal.h>

#ifndef ZEPHYR_INCLUDE_MIDI_SERIAL_INTERNAL_H_
#define ZEPHYR_INCLUDE_MIDI_SERIAL_INTERNAL_H_

#define COMPAT_SERIAL_DEVICE nordic_nrf_uarte
#define COMPAT_MIDI_DEVICE midi_device
#define COMPAT_MIDI_SERIAL_IN_DEVICE midi_serial_in_device
#define COMPAT_MIDI_SERIAL_OUT_DEVICE midi_serial_out_device

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
/*#define COMPAT_COUNT(node_id, compat)	\
	COND_NODE_HAS_COMPAT_CHILD(node_id, compat, \
		(NUM_VA_ARGS_LESS_1(COMPAT_CHILDREN_LIST(node_id, compat),)), (0))*/

/* Number of MIDI serial devices*/
#define SERIAL_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_SERIAL_DEVICE)
#define MIDI_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_DEVICE)
#define MIDI_SERIAL_IN_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_SERIAL_IN_DEVICE)
#define MIDI_SERIAL_OUT_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_SERIAL_OUT_DEVICE)

/* Get MIDI serial device node ID */
#define MIDI_DEV_N_ID(dev)	        DT_INST(dev, COMPAT_MIDI_DEVICE)
#define SERIAL_DEV_N_ID(dev)	    DT_PARENT(MIDI_DEV_N_ID(dev))
#define SERIAL_IN_DEV_N_ID(dev)	    DT_INST(dev, COMPAT_MIDI_SERIAL_IN_DEVICE)
#define SERIAL_OUT_DEV_N_ID(dev)	DT_INST(dev, COMPAT_MIDI_SERIAL_OUT_DEVICE)


#endif /* ZEPHYR_INCLUDE_MIDI_SERIAL_INTERNAL_H_ */