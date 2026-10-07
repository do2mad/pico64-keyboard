# Pico64 Keyboard – C64 keyboard matrix adapter with Raspberry Pi Pico 2 W
# Build: mkdir build && cd build && cmake .. && make -j
#
# Bluepad32 (Bluetooth keyboards / gamepads) is downloaded automatically at a
# fixed, tested revision. To use a local copy: cmake -DBLUEPAD32_ROOT=/path/to/bluepad32 ..
cmake_minimum_required(VERSION 3.18)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

# Bluepad32 uses the BTstack that ships with the Pico SDK
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)

project(pico64 C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

# --- Bluepad32 -------------------------------------------------------------
set(BLUEPAD32_ROOT "" CACHE PATH "Local Bluepad32 checkout (empty = download)")

# sdkconfig.h and btstack_config.h are in this folder - Bluepad32 needs them too
include_directories(${CMAKE_CURRENT_LIST_DIR})

if(BLUEPAD32_ROOT)
    add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)
else()
    include(FetchContent)
    FetchContent_Declare(bluepad32
        GIT_REPOSITORY https://github.com/ricardoquesada/bluepad32.git
        GIT_TAG        e9b755faabc240585da42e6d26164bb2cdd064d3
        GIT_SUBMODULES ""            # its own BTstack copy is not needed
        SOURCE_SUBDIR  src/components/bluepad32
    )
    FetchContent_MakeAvailable(bluepad32)
endif()

# --- Pico64 ----------------------------------------------------------------
add_executable(pico64
    main.c
    matrix.c
    keys.c
    keymap.c
    joystick.c
    ble_service.c
    input_bt.c
    textfeed.c
)

target_include_directories(pico64 PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pico64
    pico_stdlib
    pico_multicore
    pico_flash
    pico_btstack_ble
    pico_btstack_classic
    pico_btstack_cyw43
    pico_cyw43_arch_none
    bluepad32
)

pico_btstack_make_gatt_header(pico64 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/pico64.gatt")

# USB serial console (115200): status output, commands and "type text"
pico_enable_stdio_usb(pico64 1)
pico_enable_stdio_uart(pico64 0)

pico_add_extra_outputs(pico64)
