# Änderungen / Changelog

Pico64 Keyboard. Neueste Version zuerst.

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
