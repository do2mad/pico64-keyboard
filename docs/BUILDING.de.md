# Firmware selbst bauen

*English version: [BUILDING.md](BUILDING.md)*

Das braucht man nur, wenn man die Firmware ändern will – eine fertige `.uf2` hängt an jedem
[Release](https://github.com/do2mad/pico64-keyboard/releases), und die
[Flash-Seite](https://1mhz.de/Pico64/) spielt sie direkt aus dem Browser auf.

Voraussetzungen: CMake, Ninja (oder Make), die Arm-GNU-Toolchain (`arm-none-eabi-gcc`), das
**Raspberry Pi Pico SDK 2.2.0** mit den Submodulen `cyw43-driver` und `btstack` sowie git.
[Bluepad32](https://github.com/ricardoquesada/bluepad32) lädt CMake selbst herunter (feste
Revision `e9b755f`); für eine lokale Kopie `-DBLUEPAD32_ROOT=/pfad/zu/bluepad32` angeben.

```sh
git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git submodule update --init lib/cyw43-driver lib/btstack && cd ..
export PICO_SDK_PATH=$PWD/pico-sdk

git clone https://github.com/do2mad/pico64-keyboard.git
cd pico64-keyboard/firmware
cmake -S . -B build -G Ninja
ninja -C build            # -> build/pico64.uf2
```

Am Mac: `brew install cmake ninja` und `brew install --cask gcc-arm-embedded`.
Oder die Erweiterung „Raspberry Pi Pico“ für VS Code nehmen, die alles installiert.

## Überblick über den Quelltext

| Datei | Inhalt |
|---|---|
| `matrix.c/.h` | Pinbelegung, Schleife auf Kern 1 (läuft aus dem RAM), Tabellen, Open-Drain-Ausgänge, Joystick-Leitungen |
| `keys.c/.h` | Tasten-Verwaltung: führt App, Tastaturen, Gamepad-Tasten und Kabeleingang zusammen, Haltezeiten, Tastenlog |
| `ble_service.c/.h` | BLE-Dienst für die App (GATT, Advertising), LED |
| `input_bt.c/.h` | Bluepad32-Plattform: Bluetooth-Tastaturen und Gamepads, Port-Zuordnung |
| `keymap.c/.h` | HID-Tastatur → C64-Tasten (symbolisch deutsch / positionsgleich) |
| `joystick.c/.h` | Joystick-Ports 1 und 2, Mindesthaltezeit |
| `textfeed.c/.h` | tippt Text in BT-64-Makrosyntax (`~ret~`, `~clr~`, `~f1~` …) |
| `wire.c/.h` | Kabeleingang vom CBM2 Keyboard (UART an GP17) |
| `usb_kbd.c/.h` | USB-Gerät: serielle Konsole (CDC) und Tastatur (HID) |
| `usb_mode.c/.h` | USB-Tastatur ohne C64: fragt die echte C64-Tastatur ab |
| `main.c` | Start, Erkennung des Betriebsmodus, USB-Konsole |
| `pico64.gatt` | GATT-Datenbank (der Build macht daraus `pico64.h`) |
| `btstack_config.h` | BTstack-Konfiguration (Classic + LE, aus dem Bluepad32-Beispiel) |
| `sdkconfig.h` | Bluepad32-Konfiguration |
| `tusb_config.h` | TinyUSB-Konfiguration |
| `CMakeLists.txt` | Build für `PICO_BOARD=pico2_w`, holt Bluepad32 |
