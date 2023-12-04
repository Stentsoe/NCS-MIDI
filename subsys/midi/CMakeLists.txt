#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()
# zephyr_library_sources(
# 	midi_parser.c
# )

zephyr_include_directories(include)


if(CONFIG_MIDI)
  zephyr_library_include_directories(include)
  zephyr_library_sources(midi_msg.c)
  zephyr_library_sources_ifdef(CONFIG_TEST_GPIO_OUT             test_gpio_out.c)
  zephyr_library_sources_ifdef(CONFIG_TEST_GPIO_IN              test_gpio_in.c)
  zephyr_library_sources_ifdef(CONFIG_MIDI_PARSER               midi_parser.c)
  zephyr_library_sources_ifdef(CONFIG_MIDI_SYNC   	            midi_sync.c)
  zephyr_library_sources_ifdef(CONFIG_MIDI_SERIAL               midi_serial.c)
  zephyr_library_sources_ifdef(CONFIG_MIDI_BLUETOOTH_PERIPHERAL midi_bluetooth_peripheral.c)
  zephyr_library_sources_ifdef(CONFIG_MIDI_BLUETOOTH_CENTRAL    midi_bluetooth_central.c)
  zephyr_library_sources_ifdef(CONFIG_MIDI_ISO_BROADCASTER      midi_iso_broadcaster.c)
  zephyr_library_sources_ifdef(CONFIG_MIDI_ISO_RECEIVER         midi_iso_receiver.c)
endif()