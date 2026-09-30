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
- **Core 0** runs Bluetooth (BTstack), receives the key states from the app, holds every key
  press for 60 ms and every release for 30 ms (the KERNAL scans every 1/60 s) and rebuilds the
  table on every change. It also runs the text feed and the USB console.

Because the table covers **every** column pattern, programs that select several columns at once
(e.g. `$DC00 = 0` for "any key?") work, and any number of keys can be pressed at the same time.

### The one rule

PB0–PB7 and RESTORE are **only pulled to GND or released** (open drain). The Pico never drives
them high. That keeps it compatible with the real keyboard, the joystick ports (which share
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
| 13 | PA0 | 1 kΩ | GP0 | 1 |
| 19 | PA1 | 1 kΩ | GP1 | 2 |
| 18 | PA2 | 1 kΩ | GP2 | 4 |
| 17 | PA3 | 1 kΩ | GP3 | 5 |
| 16 | PA4 | 1 kΩ | GP4 | 6 |
| 15 | PA5 | 1 kΩ | GP5 | 7 |
| 14 | PA6 | 1 kΩ | GP6 | 9 |
| 20 | PA7 | 1 kΩ | GP7 | 10 |
| 12 | PB0 | 330 Ω | GP8 | 11 |
| 11 | PB1 | 330 Ω | GP9 | 12 |
| 10 | PB2 | 330 Ω | GP10 | 14 |
| 5 | PB3 | 330 Ω | GP11 | 15 |
| 8 | PB4 | 330 Ω | GP12 | 16 |
| 7 | PB5 | 330 Ω | GP13 | 17 |
| 6 | PB6 | 330 Ω | GP14 | 19 |
| 9 | PB7 | 330 Ω | GP15 | 20 |
| 3 | RESTORE | 330 Ω | GP16 | 21 |
| 4 | +5 V | Schottky 1N5817 (anode at the C64) | VSYS | 39 |
| 1 | GND | – | GND | 38 |
| 2 | key (no pin) | | | |

CN1 pinout after Ruud Baltissen ("Commodore 64 keyboard connector and keyboard matrix").
**Check it with a multimeter before soldering** – GND (1), +5 V (4) and RESTORE (3) are easy to
verify, PA/PB by continuity to CIA 1 (pins 2–9 = PA0–PA7, 10–17 = PB0–PB7).

### Why these resistors

- **1 kΩ in the PA lines:** protection only (limits current in case of a fault or at power-up).
  The inputs draw practically no current, the level is not affected.
- **330 Ω in the PB lines and RESTORE:** protection, but small enough to keep the low level well
  below the CIA's input threshold against its pull-ups. The firmware drives these pins with
  12 mA strength. If a key is sometimes not recognised, measure the PB line at CN1 while the key
  is held in the app: it should be below 0.4 V. If not, use 100 Ω instead.
- **Schottky diode:** the Pico is powered from the C64's +5 V. When USB is connected as well,
  the diode stops the Pico from feeding the C64 through VSYS (VBUS→VSYS already has a diode on
  the Pico board).

The Pico must be running whenever the C64 is running (5 V tolerance needs supply voltage) –
powering it from the C64 guarantees that.

## Parts list

| Qty | Part | Note |
|---|---|---|
| 1 | Raspberry Pi Pico 2 W | with pin headers or soldered directly |
| 8 | Resistor 1 kΩ | or 1 × resistor network 8 × 1 kΩ, isolated (e.g. Bourns 4116R-1-102) |
| 9 | Resistor 330 Ω | or 1 × 4116R-1-331 + 1 single resistor |
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
7. **Games:** a few games with keyboard control, including ones that scan several columns at once.

## Known limitations / ideas

- **Reverse scanning:** a few programs drive PB and read PA. The firmware does not emulate that
  (yet); it could be added with a second table and the PA pins as open-drain outputs.
- **Pairing / allow list:** like the BT-64 extension, anyone in range can connect.
- **Joystick emulation:** the same PA/PB lines are the joystick ports – a joystick view in the
  app would be almost free.
