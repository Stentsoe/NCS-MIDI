/*
 * USB midi class core header
 *
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief USB Audio Device Class public header
 *
 * Header follows below documentation:
 * - USB Device Class Definition for Audio Devices (audio10.pdf)
 *
 * Additional documentation considered a part of USB Audio v1.0:
 * - USB Device Class Definition for Audio Data Formats (frmts10.pdf)
 * - USB Device Class Definition for Terminal Types (termt10.pdf)
 */

#ifndef ZEPHYR_INCLUDE_USB_CLASS_AUDIO_H_
#define ZEPHYR_INCLUDE_USB_CLASS_AUDIO_H_

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/** Audio Interface Subclass Codes
 * Refer to Table A-2 from audio10.pdf
 */
enum usb_audio_int_subclass_codes {
	USB_AUDIO_AUDIOCONTROL		= 0x01,
	USB_AUDIO_MIDISTREAMING		= 0x03
};

/** Audio Class-Specific AC Interface Descriptor Subtypes
 * Refer to Table A-5 from audio10.pdf
 */
enum usb_audio_cs_ac_int_desc_subtypes {
	USB_AUDIO_HEADER				= 0x01
};

enum usb_midi_cs_desc_types	{
	USB_CS_GR_TRM_BLOCK = 0x26
};

enum usb_midi_cs_int_desc_subtypes {
	USB_MS_DESCRIPTOR_UNDEFINED	= 0x00,
	USB_MS_HEADER	= 0x01,
	USB_MIDI_IN_JACK = 0x02,
	USB_MIDI_OUT_JACK = 0x03
};

enum usb_midi_cs_ep_desc_subtypes {
	USB_DESCRIPTOR_UNDEFINED = 0x00,
	USB_MS_GENERAL = 0x01,
	USB_MS_GENERAL_2_0	= 0x02
};

enum usb_midi_cs_gt_block_desc_subtypes	{
	USB_GR_TRM_BLOCK_UNDEFINED = 0x00,
	USB_GR_TRM_BLOCK_HEADER = 0x01,
	USB_GR_TRM_BLOCK = 0x02
};

enum usb_midi_jack_direction {
	JACK_IN,
	JACK_OUT
};




struct usb_midi_ops {

};

/**
 * @brief Register the USB Audio device and make it useable.
 *	  This must be called in order to make the device work
 *	  and respond to all relevant requests.
 *
 * @param dev USB Audio device
 * @param ops USB audio callback structure. Callback are used to
 *	      inform the user about what is happening
 */
// void usb_midi_register(const struct device *dev,
// 			const struct usb_midi_ops *ops);


void test_func(void);

#endif /* ZEPHYR_INCLUDE_USB_CLASS_AUDIO_H_ */
