# Pico64 – C64 keyboard matrix adapter with Raspberry Pi Pico 2 W
# Build: mkdir build && cd build && cmake .. && make -j
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(pico64 C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(pico64
    main.c
    matrix.c
    ble_service.c
    textfeed.c
)

target_include_directories(pico64 PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pico64
    pico_stdlib
    pico_multicore
    pico_btstack_ble
    pico_btstack_cyw43
    pico_cyw43_arch_none
)

pico_btstack_make_gatt_header(pico64 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/pico64.gatt")

# USB serial console (115200): status output and "type text" for testing
pico_enable_stdio_usb(pico64 1)
pico_enable_stdio_uart(pico64 0)

pico_add_extra_outputs(pico64)
