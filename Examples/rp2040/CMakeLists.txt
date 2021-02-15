cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()

#set("PICO_SDK_PATH" "~/gith/pico-sdk")
include(pico_sdk_import.cmake)

project(ustd_rp2040)
include_directories(../.. ../../../../ustd ../../../ustd ustd)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(rp2040 src/rp2040.cpp)
# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(rp2040 pico_stdlib pico_malloc)

# enable usb output, disable uart output
pico_enable_stdio_usb(rp2040 1)
pico_enable_stdio_uart(rp2040 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(rp2040)

