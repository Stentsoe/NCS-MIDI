

add_subdirectory_ifdef(CONFIG_BT    bluetooth)
add_subdirectory_ifdef(CONFIG_USB_DEVICE_STACK   usb)
add_subdirectory_ifdef(CONFIG_MIDI midi)


include_directories(nrf_midi/subsys/usb)
