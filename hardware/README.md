# Hardware (PCB)

[Deutsch](README.de.md)

PCB for **Pico64 Keyboard**: a pass-through for the C64 keyboard connector CN1 with a
Raspberry Pi Pico 2 W soldered **flat into a cut-out** (castellated edges), 17 × 100 Ω and a
Schottky diode – it fits under the case top. Tested in a breadbin C64.

| | |
|---|---|
| ![assembled](images/pcb-assembled.jpg) | ![in the C64](images/pcb-in-c64.jpg) |
| assembled | in the C64 (on CN1, keyboard cable on top) |
| ![3D top](images/pcb-3d-top.png) | ![bare](images/pcb-bare.jpg) |
| 3D view (KiCad) | bare board |

## Files

| File | |
|---|---|
| [gerber/pico64-keyboard-gerber-v1.1.zip](gerber/pico64-keyboard-gerber-v1.1.zip) | Gerber + drill files, ready for the PCB maker (2 layers, 1.6 mm) |
| [Pico64-keyboard-bom.csv](Pico64-keyboard-bom.csv) | parts list |

v1.1: no holes at pin 2 of J1/J2 (the key position of the connector) and pin numbers on the
silkscreen. The boards in the photos are v1.0 – otherwise identical.

## Parts

| Qty | Part | |
|---|---|---|
| 1 | Raspberry Pi Pico 2 W | without headers, soldered flat by its castellated edges |
| 17 | resistor 100 Ω, 1/4 W (DIN0204, 7.62 mm) | R1–R17 |
| 1 | Schottky diode 1N5817 (DO-41) | D1, anode at the C64 +5 V |
| 1 | female header 1 × 20, 2.54 mm | J2, onto CN1 on the mainboard |
| 1 | male header 1 × 20, 2.54 mm | J1, takes the keyboard cable plug |

Board 53 × 68 mm (incl. the connector arm), 2 layers, 1.6 mm. Soldering the Pico into the cut-out takes some care –
tin the pads, then solder edge by edge.

Wiring, pin table and safety notes: [../docs/HARDWARE.md](../docs/HARDWARE.md).

**Before plugging it in:** check the CN1 pinout of your C64 with a multimeter (GND pin 1,
+5 V pin 4, RESTORE pin 3).

## License

CC BY-NC-SA 4.0, see [../LICENSE-HARDWARE.md](../LICENSE-HARDWARE.md).
