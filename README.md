# Pico64 Keyboard

*Deutsche Fassung: [README.de.md](README.de.md)*

A tiny adapter that turns a **Raspberry Pi Pico 2 W** into a wireless keyboard and joystick
interface for the **Commodore 64** – use your phone with the
[Blue-64 Keyboard app](https://github.com/do2mad/blue64-keyboard-ios) (iPhone and Android),
a **Bluetooth keyboard** or a **Bluetooth gamepad** (e.g. 8BitDo).

**Get the app:** iPhone – [TestFlight beta](https://testflight.apple.com/join/jEn4tsP7) · Android – [APK in the latest release](https://github.com/do2mad/blue64-keyboard-ios/releases/latest)

The Pico plugs in between the C64 mainboard and the keyboard cable and emulates the keyboard
matrix in software. Apart from the Pico you only need **17 resistors and one diode** – no level
shifters and no crosspoint switch, because GPIO 0–25 of the RP2350 are 5 V tolerant.
The original keyboard and both joysticks keep working.

![Wiring](docs/wiring.png)

## Features

- Emulates the complete 8×8 keyboard matrix plus RESTORE – any number of keys at the same time
- Reacts within about 70 ns to every column the C64 selects (core 1 runs a lookup table from RAM),
  so programs that scan several columns at once work too
- Same Bluetooth LE service as the BT-64 / Blue-64 firmware with the BLE keyboard service –
  the Blue-64 Keyboard app works unchanged (the Pico shows up as **Pico64**)
- Type text and short BASIC listings from the app (snippets)
- **Bluetooth keyboards** (Classic and BLE) – German QWERTZ layout typed as printed, or
  positional mapping for games (F10 switches)
- **Bluetooth gamepads** (8BitDo, Xbox, PlayStation, Switch and many more, via
  [Bluepad32](https://github.com/ricardoquesada/bluepad32)) as **joystick in port 1 or 2**
- App, keyboard and gamepads can be used at the same time; **two app devices** (e.g. iPhone and iPad) at once
- USB console: every line you type in a terminal is typed on the C64, plus a key log for debugging
- Powered by the C64, no extra power supply

## Status

**v0.12.0 – working prototype.** Built on perfboard and tested on a breadbin C64 with an
iPhone and an Android phone. Bluetooth keyboard and gamepad support is new in v0.10.0 and still
needs testing with more devices. A KiCad PCB is in progress (see [hardware/](hardware/)).

> **Hardware change in v0.10:** all 17 resistors are now 100 Ω (PA0–PA7 were 1 kΩ). Joystick
> port 2 needs this – everything else also works with the old values. See [docs/HARDWARE.md](docs/HARDWARE.md).

## Getting started

1. **Build the adapter** – wiring, parts list and safety notes: [docs/HARDWARE.md](docs/HARDWARE.md).
   Check the CN1 pinout of your C64 with a multimeter before soldering.
2. **Flash the firmware** – hold BOOTSEL, plug the Pico into your computer, drag
   `pico64-keyboard-vX.Y.Z.uf2` from the [latest release](https://github.com/do2mad/pico64-keyboard/releases/latest)
   onto the `RP2350` drive. Do this with the Pico **out of the C64** or with the C64 switched off.
3. **Plug it in** between CN1 and the keyboard cable, switch the C64 on – the Pico LED blinks
   (nothing connected yet).
4. **Connect with the app** – [Blue-64 Keyboard](https://github.com/do2mad/blue64-keyboard-ios)
   finds the Pico by itself, the LED stays on while connected.
5. **Or pair a Bluetooth keyboard / gamepad** – see below.

Building the firmware yourself: [docs/BUILDING.md](docs/BUILDING.md).
Bluetooth protocol: [docs/PROTOCOL.md](docs/PROTOCOL.md).

## Bluetooth keyboard and gamepad

The Pico looks for keyboards and gamepads all the time. To pair a new one, put it into pairing
mode – it connects by itself within a few seconds (no PIN, no app needed). Paired devices
reconnect automatically when switched on. Up to 4 Bluetooth connections at the same time
(apps, keyboards and gamepads together).

- **Keyboard:** usually hold the Bluetooth / pairing key (or Fn + a channel key) for 3 s, see
  the keyboard's manual. If the keyboard asks for a code, type `0000` and Enter.
- **8BitDo gamepads:** switch on in **Android / D-input mode** (many models: hold **B + Start**)
  or **Switch mode** (**Y + Start**), then hold the pair button for 3 s. Modes differ per model –
  see the 8BitDo manual and the [Bluepad32 list of supported controllers](https://bluepad32.readthedocs.io/).
- **Other gamepads** (Xbox, PlayStation, Switch Pro …): normal pairing mode.

### Keyboard mapping

Two modes, **F10** switches (the USB console shows the current mode):

| Mode | What it does |
|---|---|
| **Symbolic** (default) | Types what is printed on a **German** keyboard: Shift+0 gives `=`, Shift+2 `"`, AltGr+Q `@`, Z and Y in the right place. Characters the C64 doesn't have (ä ö ü ß € …) are ignored. |
| **Positional** | Every key acts like the C64 key at the same place, SHIFT stays SHIFT – best for games and for the graphic characters. |

| PC key | C64 key |
|---|---|
| Esc | RUN/STOP |
| Tab | CTRL |
| Ctrl | CTRL |
| Alt, Windows/Cmd | C= |
| Backspace, Del | INST/DEL |
| Home | CLR/HOME |
| Insert | £ |
| F1 – F8 | F1 – F8 |
| F9, Page Up, Pause | RESTORE |
| Cursor keys | CRSR (up/left with SHIFT automatically) |
| F10 | switch mapping |

### Gamepad

| Gamepad | C64 |
|---|---|
| D-pad or left stick | joystick directions |
| A, B | fire |
| X | SPACE |
| Y | RETURN |
| Select / − | RUN/STOP |
| Start / + | F1 |
| L1 + R1 together | swap joystick port (1 ↔ 2) |

The first gamepad plays on **port 2** (most games), a second one on port 1.

## USB keyboard (without a C64)

Take the board out of the C64, plug the C64 keyboard cable into it and connect the Pico via USB
to a Mac, PC, Raspberry Pi or MiSTer: the **real C64 keyboard becomes a USB keyboard**. The app
and Bluetooth keyboards work at the same time and are sent via USB too.

The Pico decides when it starts: without USB power it can only be powered by the C64 → C64
mode. With USB power it listens for 0.6 s whether a C64 scans the keyboard → C64 mode,
otherwise USB keyboard. If a C64 starts scanning later, the Pico restarts in C64 mode by itself.
A C64 that is switched off but still connected pulls the lines low – the Pico then stays in C64
mode (the USB keyboard only works with the board outside the C64).

Choose the output with **RESTORE + F key** (stored):

| Keys | Output |
|---|---|
| RESTORE + F1 | MiSTer, C64 core (keys as the core expects them; RESTORE alone = F11) |
| RESTORE + F3 | PC, German layout |
| RESTORE + F5 | PC, US layout |
| RESTORE + F7 | Mac, German layout |
| RESTORE + F8 (SHIFT + F7) | Mac, US layout |

PC / Mac: types what is printed on the key (SHIFT + 2 = `"`, SHIFT + `:` = `[`). DEL = Backspace,
INST = Insert, CRSR = cursor keys, F1–F8, HOME / CLR = Home / End, RUN/STOP = Esc, ← = Tab,
CTRL = Ctrl, C= = Win / Cmd, £ = `\`. Like the original, the keyboard has no diodes, so some
three-key combinations show a phantom key.

In the USB console `!u` shows the mode, `!u0` … `!u4` choose the output.

## USB console

Connect the Pico via USB while it sits in the C64 (the diode protects both sides) and open a
terminal with 115200 baud, e.g. `screen /dev/tty.usbmodem* 115200` on a Mac.

- Every line you type is typed on the C64, followed by RETURN (letters unshifted, tokens like
  `~clr~`, `~home~`, `~f1~` work).
- A line with just `d` switches the key log on or off: every key state received via Bluetooth
  and every state applied to the matrix, with timestamps, plus the connection interval.
- `!s` status (app, keyboards, gamepads, joystick ports), `!p` switch keyboard mapping,
  `!b` search for new Bluetooth devices on/off, `!x` forget all paired keyboards / gamepads,
  `!u` USB keyboard mode (see above).

## Safety

- The Pico **never drives a C64 line high** – PB0–PB7, RESTORE and (for joystick 2) PA0–PA4
  are only pulled to GND or released (open drain), like a key switch or a joystick does.
- Use GP0–GP22 only. GP26–GP28 are **not** 5 V tolerant, GP23–25 and GP29 belong to the radio.
- 5 V tolerance only applies while the Pico is powered – which it always is, because it is
  powered by the C64.
- Use at your own risk. Check your wiring twice.

## Related projects

- [Blue-64 Keyboard](https://github.com/do2mad/blue64-keyboard-ios) – the app (iPhone and Android)
- [Pico USB Keyboard](https://github.com/do2mad/pico-usb-keyboard) – the same app as a USB keyboard:
  a plain Pico 2 W without wiring, for the MiSTer C64 core, PC, Mac or Raspberry Pi
- [BT-64 with BLE keyboard service](https://github.com/do2mad/BT-64/tree/ble-keyboard) – the
  firmware extension for the BT-64 / Blue-64 adapter

## Credits & license

- Firmware © 2026 Martin Oswald (do2mad) – [1mhz.de](https://1mhz.de), [MIT License](LICENSE)
- Hardware (schematic, PCB) © 2026 Martin Oswald,
  [CC BY-NC-SA 4.0](LICENSE-HARDWARE.md) – non-commercial use
- Uses the Raspberry Pi Pico SDK, BTstack, the CYW43 driver and
  [Bluepad32](https://github.com/ricardoquesada/bluepad32) by Ricardo Quesada – see [NOTICE](NOTICE)
- The Bluetooth service follows the protocol of the BLE keyboard extension for
  [BT-64 / Blue-64](https://github.com/sideprojectslab/BT-64) by SideProjectsLab
- C64 keyboard connector pinout after Ruud Baltissen

Changes: [CHANGELOG.md](CHANGELOG.md)

Commodore and Commodore 64 are trademarks of their respective owners. This project is not
affiliated with or endorsed by them, by Raspberry Pi Ltd. or by SideProjectsLab.
