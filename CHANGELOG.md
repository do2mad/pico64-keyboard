# Änderungen / Changelog

Pico64 Keyboard. Neueste Version zuerst.

## v0.12.0 – 2026-10-15

### Deutsch

- **USB-Tastatur ohne C64:** Platine außerhalb des C64, Tastaturkabel dran, USB an Mac/PC/MiSTer – die echte C64-Tastatur wird zur USB-Tastatur. Ausgabe mit RESTORE + F1 (MiSTer C64-Core), F3 (PC deutsch), F5 (PC US), F7 (Mac deutsch), F8 (Mac US). App und Bluetooth-Tastaturen gehen dann auch per USB raus.
- Automatische Erkennung beim Start (USB-Spannung, fragt ein C64 die Tastatur ab?); startet ein C64 später, wechselt der Pico von selbst in den C64-Modus. Hardware unverändert.
- Konsole: `!u` Modus anzeigen, `!u0`–`!u4` Ausgabe wählen.

### English

- **USB keyboard without a C64:** board outside the C64, keyboard cable connected, USB to a Mac/PC/MiSTer – the real C64 keyboard becomes a USB keyboard. Output with RESTORE + F1 (MiSTer C64 core), F3 (PC German), F5 (PC US), F7 (Mac German), F8 (Mac US). The app and Bluetooth keyboards are sent via USB too.
- Automatic detection at start (USB power, does a C64 scan the keyboard?); if a C64 starts later, the Pico switches to C64 mode by itself. No hardware change.
- Console: `!u` shows the mode, `!u0`–`!u4` choose the output.

## v0.11.1 – 2026-10-15

### Deutsch

- Die **rechte SHIFT-Taste** der App wird jetzt als rechte SHIFT-Taste des C64 gedrückt (vorher immer die linke). Braucht die App ab 1.1.0 (7); mit älteren Apps bleibt es bei der linken.

### English

- The app's **right SHIFT key** now presses the C64's right SHIFT key (was always the left one). Needs the app 1.1.0 (7) or newer; older apps keep using the left one.

## v0.11.0 – 2026-10-08

### Deutsch

- **Zwei App-Geräte gleichzeitig:** z. B. iPhone und iPad (oder zwei Spieler) sind zugleich verbunden. Die Tasten beider Geräte werden zusammengelegt, wie bei zwei Tastaturen. Trennt sich ein Gerät, werden nur dessen Tasten losgelassen. Text sendet immer nur ein Gerät, das andere wartet kurz.
- Zusammen höchstens 4 Bluetooth-Verbindungen (Apps, Tastaturen, Gamepads).

### English

- **Two app devices at the same time:** e.g. iPhone and iPad (or two players) can be connected together. Their keys are combined, like two keyboards. When one disconnects, only its keys are released. Only one device sends text at a time, the other one waits briefly.
- Up to 4 Bluetooth connections in total (apps, keyboards, gamepads).

## v0.10.0 – 2026-10-01

### Deutsch

- **Bluetooth-Tastaturen** (Classic und BLE) direkt am Pico, ohne App. Zwei Belegungen, F10 schaltet um: symbolisch (deutsches QWERTZ, getippt wird, was aufgedruckt ist) und positionsgleich (für Spiele).
- **Bluetooth-Gamepads** (z. B. 8BitDo) als **Joystick an Port 1 oder 2**: Steuerkreuz/Stick, A/B = Feuer, X = SPACE, Y = RETURN, Select = RUN/STOP, Start = F1, L1+R1 = Port wechseln. Erstes Gamepad an Port 2.
- App, Tastatur und Gamepads gleichzeitig; bis zu 4 Bluetooth-Geräte. Gekoppelte Geräte verbinden sich automatisch wieder.
- USB-Konsole: `!s` Status, `!p` Belegung, `!b` Gerätesuche an/aus, `!x` Kopplungen löschen.
- Nutzt [Bluepad32](https://github.com/ricardoquesada/bluepad32) (Apache-2.0), wird beim Bauen automatisch geladen.
- Hardware: **alle 17 Widerstände 100 Ω** – jetzt auch PA0–PA7 (vorher 1 kΩ), nötig für Joystick-Port 2. Stückliste: 2 × 4116R-1-101 + 1 Einzelwiderstand.

### English

- **Bluetooth keyboards** (Classic and BLE) directly on the Pico, no app needed. Two mappings, F10 switches: symbolic (German QWERTZ, types what is printed on the key) and positional (for games).
- **Bluetooth gamepads** (e.g. 8BitDo) as **joystick in port 1 or 2**: D-pad/stick, A/B = fire, X = SPACE, Y = RETURN, Select = RUN/STOP, Start = F1, L1+R1 = swap port. First gamepad on port 2.
- App, keyboard and gamepads at the same time; up to 4 Bluetooth devices. Paired devices reconnect automatically.
- USB console: `!s` status, `!p` mapping, `!b` device search on/off, `!x` forget pairings.
- Uses [Bluepad32](https://github.com/ricardoquesada/bluepad32) (Apache-2.0), downloaded automatically by the build.
- Hardware: **all 17 resistors 100 Ω** – now PA0–PA7 too (were 1 kΩ), needed for joystick port 2. Parts: 2 × 4116R-1-101 + 1 single resistor.

## v0.9.1 – 2026-10-01

### Deutsch

- Hardware: Widerstände in PB0–PB7 und RESTORE von 330 Ω auf **100 Ω** geändert. Mit 330 Ω lag der Low-Pegel am C64 bei 0,69 V (CIA-Grenze 0,8 V) – kurze Tastendrücke gingen manchmal verloren. Mit 100 Ω ca. 0,3 V.
- Firmware unverändert.

### English

- Hardware: resistors in PB0–PB7 and RESTORE changed from 330 Ω to **100 Ω**. With 330 Ω the low level at the C64 was 0.69 V (CIA limit 0.8 V) – short key presses were sometimes missed. With 100 Ω about 0.3 V.
- Firmware unchanged.

## v0.9.0 – 2026-09-30

### Deutsch

Erste öffentliche Version (funktionierender Prototyp auf Lochraster).

- Komplette C64-Tastaturmatrix plus RESTORE mit dem Pico 2 W nachgebildet, beliebig viele Tasten gleichzeitig
- Kern 1 reagiert in ca. 70 ns auf die Spaltenauswahl des C64 (Tabelle im RAM)
- Bluetooth-LE-Dienst kompatibel zur Blue-64-Keyboard-App (iPhone und Android), Erweiterung für volle Matrix
- Tastendruck 60 ms / Loslassen 30 ms gehalten, Warteschlange – auch kurze Tipper kommen an
- Fordert 15–30 ms Verbindungsintervall an (schnell auch mit iPhone)
- Text tippen lassen, USB-Konsole mit Tasten-Protokoll
- Nur 17 Widerstände und 1 Diode nötig, Versorgung aus dem C64

### English

First public release (working prototype on perfboard).

- Emulates the complete C64 keyboard matrix plus RESTORE with a Pico 2 W, any number of keys at once
- Core 1 reacts in about 70 ns to the column selected by the C64 (lookup table in RAM)
- Bluetooth LE service compatible with the Blue-64 Keyboard app (iPhone and Android), full-matrix extension
- Key presses held 60 ms, releases 30 ms, queued – even short taps arrive
- Requests a 15–30 ms connection interval (fast with iPhones too)
- Text feed, USB console with key log
- Only 17 resistors and 1 diode needed, powered by the C64
