cmake_minimum_required(VERSION 3.13)

add_executable(usb_can_converter)

target_sources(usb_can_converter PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        ${CMAKE_CURRENT_LIST_DIR}/can2040/src/can2040.c
        ${CMAKE_CURRENT_LIST_DIR}/crc_ccitt.c
        )

# Make sure TinyUSB can find tusb_config.h
target_include_directories(usb_can_converter PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/can2040/src/
        PUBLIC E:\\pico\\pico-sdk\\src\\rp2_common\\cmsis\\stub\\CMSIS\\Device\\RaspberryPi\\RP2040\\Include
        PUBLIC E:\\pico\\pico-sdk\\src\\rp2_common\\cmsis\\stub\\CMSIS\\Core\\Include
        )

# In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
# for TinyUSB device support and tinyusb_board for the additional board support library used by the example
target_link_libraries(usb_can_converter PUBLIC pico_stdlib tinyusb_device tinyusb_board hardware_irq cmsis_core)

# Uncomment this line to enable fix for Errata RP2040-E5 (the fix requires use of GPIO 15)
#target_compile_definitions(dev_hid_composite PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

pico_add_extra_outputs(usb_can_converter)

# add url via pico_set_program_url
example_auto_set_url(usb_can_converter)
