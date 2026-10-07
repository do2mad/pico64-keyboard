# Hardware

## How it works

The C64 scans its keyboard through CIA 1:

1. It writes `$DC00` (port **A**, PA0–PA7) and pulls one or more **columns** low.
2. A few microseconds later it reads `$DC01` (port **B**, PB0–PB7). Every pressed key in an
   active column connects its **row** to that column and pulls the PB line low.

The Pico does exactly what the key switches would do:

- **Core 1** reads PA0–PA7 in a tight loop and looks up, in a table with 256 entries, which PB
  lines must be low for this column pattern. Reaction time is about 60–80 ns (9 instructions at
  150 MHz, code in RAM). The C64 reads several microseconds later.
- **Core 0** runs Bluetooth (BTstack + Bluepad32): the app service, Bluetooth keyboards and
  gamepads. It holds every key change long enough for the KERNAL (which scans every 1/60 s) and
  rebuilds the table on every change. It also runs the text feed and the USB console.

**Joysticks** use the same lines: port 1 = PB0–PB4, port 2 = PA0–PA4 (up, down, left, right,
fire), a real joystick simply shorts them to GND. Gamepad port 1 is ORed into every table entry;
for port 2 the Pico pulls PA0–PA4 low itself – exactly what a joystick in port 2 does.

Because the table covers **every** column pattern, programs that select several columns at once
(e.g. `$DC00 = 0` for "any key?") work, and any number of keys can be pressed at the same time.

### The one rule

All C64 lines are **only pulled to GND or released** (open drain): PB0–PB7 and RESTORE for keys
and joystick 1, PA0–PA4 for joystick 2. The Pico never drives any of them high. That keeps it compatible with the real keyboard, the joystick ports (which share
these lines) and the CIA – and no current flows into the C64 when the C64 is off and the Pico is
powered via USB.

## Why the Pico 2 W

| Fact | Consequence |
|---|---|
| RP2350: GPIO 0–25 are **5 V tolerant** (while the chip is powered) | PA lines can be read directly, no level shifters |
| GPIO 26–29 (ADC) are **not** 5 V tolerant | never connect anything from the C64 there |
| On the Pico 2 W, GPIO 23, 24, 25 and 29 are used by the radio module | free and 5 V tolerant: **GP0–GP22** |
| Absolute maximum 5.5 V per pin | only use the C64 with a healthy power supply |
| BTstack is part of the Pico SDK | same Bluetooth stack as the BT-64, the service is compatible |

## Wiring

![Wiring](wiring.png)

| C64 CN1 | Signal | Resistor | Pico | Pico pin |
|---|---|---|---|---|
| 13 | PA0 | 100 Ω | GP0 | 1 |
| 19 | PA1 | 100 Ω | GP1 | 2 |
| 18 | PA2 | 100 Ω | GP2 | 4 |
| 17 | PA3 | 100 Ω | GP3 | 5 |
| 16 | PA4 | 100 Ω | GP4 | 6 |
| 15 | PA5 | 100 Ω | GP5 | 7 |
| 14 | PA6 | 100 Ω | GP6 | 9 |
| 20 | PA7 | 100 Ω | GP7 | 10 |
| 12 | PB0 | 100 Ω | GP8 | 11 |
| 11 | PB1 | 100 Ω | GP9 | 12 |
| 10 | PB2 | 100 Ω | GP10 | 14 |
| 5 | PB3 | 100 Ω | GP11 | 15 |
| 8 | PB4 | 100 Ω | GP12 | 16 |
| 7 | PB5 | 100 Ω | GP13 | 17 |
| 6 | PB6 | 100 Ω | GP14 | 19 |
| 9 | PB7 | 100 Ω | GP15 | 20 |
| 3 | RESTORE | 100 Ω | GP16 | 21 |
| 4 | +5 V | Schottky 1N5817 (anode at the C64) | VSYS | 39 |
| 1 | GND | – | GND | 38 |
| 2 | key (no pin) | | | |

CN1 pinout after Ruud Baltissen ("Commodore 64 keyboard connector and keyboard matrix").
**Check it with a multimeter before soldering** – GND (1), +5 V (4) and RESTORE (3) are easy to
verify, PA/PB by continuity to CIA 1 (pins 2–9 = PA0–PA7, 10–17 = PB0–PB7).

### Why these resistors

All 17 resistors are **100 Ω**.

- **PA lines:** the Pico reads them, and pulls PA0–PA4 low for joystick port 2. Up to v0.9.1
  they were 1 kΩ – fine for reading, but too much for pulling a line low enough against the
  CIA's pull-up (joystick 2 would not work reliably). Adapters with 1 kΩ in PA still work for
  the keyboard and joystick port 1. Check: hold "up" on a gamepad assigned to port 2 and measure
  PA0 at CN1 pin 13 – below 0.4 V is good.
- **PB lines and RESTORE:** protection, but small enough to keep the low level well
  below the CIA's input threshold (V<sub>IL</sub> max. 0.8 V) against its pull-ups. The pull-ups
  on a real breadbin measured about 2.4 kΩ: with 100 Ω the line goes down to about 0.3 V, with the
  330 Ω of the first prototype only to 0.69 V – too close, some short key presses were missed.
  The firmware drives these pins with 12 mA strength and never drives them high, so 100 Ω is
  enough protection. Check: hold a key in the app and measure the PB line at CN1 – below 0.4 V is good.
- **Schottky diode:** the Pico is powered from the C64's +5 V. When USB is connected as well,
  the diode stops the Pico from feeding the C64 through VSYS (VBUS→VSYS already has a diode on
  the Pico board).

The Pico must be running whenever the C64 is running (5 V tolerance needs supply voltage) –
powering it from the C64 guarantees that.

## Parts list

| Qty | Part | Note |
|---|---|---|
| 1 | Raspberry Pi Pico 2 W | with pin headers or soldered directly |
| 17 | Resistor 100 Ω | or 2 × resistor network 8 × 100 Ω, isolated (Bourns 4116R-1-101) + 1 single resistor (RESTORE) |
| 1 | Schottky diode 1N5817 (or BAT54 SMD) | |
| 1 | Female header 1 × 20, 2.54 mm | goes onto CN1 on the mainboard |
| 1 | Male header 1 × 20, 2.54 mm | for the keyboard cable plug |
| 1 | Perfboard or small PCB | |

Material cost roughly 10–15 €.

## Mechanical

The adapter is a **pass-through**: the female header sits on CN1 on the mainboard, the male
header above it takes the keyboard cable plug, all 20 lines are connected 1:1. The Pico taps the
lines through the resistors. Mind the key pin (pin 2) so the keyboard cable cannot be plugged in
the wrong way round.

A PCB in KiCad, with the Pico lying flat, is in progress so the adapter fits under the case top –
see [../hardware/](../hardware/).

## Test plan

1. **Without the C64:** flash the firmware, the LED blinks, the USB console shows
   "Pico64 Keyboard v… starting". The app finds "Pico64" and connects (LED on).
2. **Measure without the C64:** GP8–GP15 must never be actively high (3.3 V) – measure against
   GND while pressing keys in the app.
3. **In the C64, first power only:** plug in the adapter, switch the C64 on, the LED blinks,
   the original keyboard types normally.
4. **USB console:** type `print"hello"` + RETURN → appears on the C64.
5. **App:** typing, SHIFT / C= / CTRL, RESTORE with RUN/STOP, charset switch, snippets.
6. **Joysticks:** joysticks in port 1 and 2 keep working, also while the app is connected.
7. **Bluetooth keyboard / gamepad:** pair them (see the README), type, play a game on port 2,
   swap the gamepad to port 1 with L1 + R1.
8. **Games:** a few games with keyboard control, including ones that scan several columns at once.

## Known limitations / ideas

- **Reverse scanning:** a few programs drive PB and read PA. The firmware does not emulate that
  (yet); it could be added with a second table and the PA pins as open-drain outputs.
- **Pairing / allow list:** like the BT-64 extension, anyone in range can connect with the app.
- **Joystick in the app:** the joystick emulation is there (used by gamepads) – a joystick view
  in the app would be almost free.
- **While pairing:** when a new Bluetooth device is paired, the Pico writes the key to flash and
  core 1 pauses for some milliseconds – keys held at that moment can briefly show up wrong.
- **Paddles / mouse:** not supported (they need the analogue POT lines, which are not on CN1).
