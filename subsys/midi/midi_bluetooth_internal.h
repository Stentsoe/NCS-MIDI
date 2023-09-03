/**
 * @file
 * @brief BLUETOOTH MIDI internal header
 *
 * This header file is used to store internal configuration
 * defines.
 */

#include "sys/util_macro_expansion.h"
#include <zephyr/sys/util_internal.h>

#ifndef ZEPHYR_INCLUDE_MIDI_BLUETOOTH_INTERNAL_H_
#define ZEPHYR_INCLUDE_MIDI_BLUETOOTH_INTERNAL_H_

#define COMPAT_RADIO_DEVICE nordic_nrf_radio
#define COMPAT_MIDI_BLUETOOTH_DEVICE midi_bluetooth_device
#define COMPAT_MIDI_BLUETOOTH_IN_DEVICE midi_bluetooth_in_device
#define COMPAT_MIDI_BLUETOOTH_OUT_DEVICE midi_bluetooth_out_device

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

/* Number of MIDI bluetooth devices*/
#define RADIO_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_RADIO_DEVICE)
#define MIDI_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_BLUETOOTH_DEVICE)
#define MIDI_BLUETOOTH_IN_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_BLUETOOTH_IN_DEVICE)
#define MIDI_BLUETOOTH_OUT_DEVICE_COUNT  DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_BLUETOOTH_OUT_DEVICE)

/* Get MIDI bluetooth device node ID */
#define MIDI_BLUETOOTH_DEV_N_ID(dev)	        DT_INST(dev, COMPAT_MIDI_BLUETOOTH_DEVICE)
#define RADIO_DEV_N_ID(dev)	    DT_PARENT(MIDI_BLUETOOTH_DEV_N_ID(dev))
#define MIDI_BLUETOOTH_IN_DEV_N_ID(dev)	    DT_INST(dev, COMPAT_MIDI_BLUETOOTH_IN_DEVICE)
#define MIDI_BLUETOOTH_OUT_DEV_N_ID(dev)	    DT_INST(dev, COMPAT_MIDI_BLUETOOTH_OUT_DEVICE)


#endif /* ZEPHYR_INCLUDE_MIDI_BLUETOOTH_INTERNAL_H_ */