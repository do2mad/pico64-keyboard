# Building the firmware

You only need this if you want to change the firmware – a ready-to-flash `.uf2` is attached to
every [release](https://github.com/do2mad/pico64-keyboard/releases).

Requirements: CMake, Ninja (or Make), the Arm GNU toolchain (`arm-none-eabi-gcc`) and the
**Raspberry Pi Pico SDK 2.2.0** with the `cyw43-driver` and `btstack` submodules.

```sh
git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git submodule update --init lib/cyw43-driver lib/btstack && cd ..
export PICO_SDK_PATH=$PWD/pico-sdk

git clone https://github.com/do2mad/pico64-keyboard.git
cd pico64-keyboard/firmware
cmake -S . -B build -G Ninja
ninja -C build            # -> build/pico64.uf2
```

On macOS: `brew install cmake ninja` and `brew install --cask gcc-arm-embedded`.
Alternatively use the "Raspberry Pi Pico" extension for VS Code, which installs everything.

## Source overview

| File | Content |
|---|---|
| `matrix.c/.h` | pin assignment, core 1 loop (runs from RAM), lookup tables, open-drain outputs |
| `ble_service.c/.h` | BLE service, key state queue and hold times, LED, key log |
| `textfeed.c/.h` | types text in BT-64 macro syntax (`~ret~`, `~clr~`, `~f1~` …) |
| `main.c` | start-up, USB console |
| `pico64.gatt` | GATT database (compiled into `pico64.h` by the build) |
| `btstack_config.h` | BTstack configuration, BLE peripheral only |
| `CMakeLists.txt` | build for `PICO_BOARD=pico2_w` |
