# Protokolle

*English version: [PROTOCOL.md](PROTOCOL.md)*

# Bluetooth-LE-Protokoll

Pico64 Keyboard spricht das **BT-64-BLE-Tastaturprotokoll v1** (denselben Dienst, den die
[Blue-64-Keyboard-App](https://github.com/do2mad/blue64-keyboard-ios) mit einem BT-64 / Blue-64 nutzt)
plus eine Erweiterung für vollständige Matrixzustände.

Der Pico meldet sich als **`Pico64`** mit der 128-Bit-Dienst-UUID
**`C64B0001-B1E6-4A64-9C64-6B7E3F1A2D00`**. Eine Kopplung ist nicht nötig.

| Characteristic | UUID | Eigenschaften |
|---|---|---|
| Key (Taste) | `C64B0002-B1E6-4A64-9C64-6B7E3F1A2D00` | write, write without response |
| Text | `C64B0003-B1E6-4A64-9C64-6B7E3F1A2D00` | write |
| Info | `C64B0004-B1E6-4A64-9C64-6B7E3F1A2D00` | read |
| Matrix *(Pico64-Erweiterung)* | `C64B0005-B1E6-4A64-9C64-6B7E3F1A2D00` | write, write without response |

## Key (Taste)

Zwei Bytes: `[flags, key]`

| flags-Bit | Bedeutung |
|---|---|
| 0 | SHIFT (linkes SHIFT) |
| 1 | C= (Commodore-Taste) |
| 2 | CTRL |
| 3 | RESTORE |
| 4 | rechtes SHIFT – nur zusammen mit Bit 0. Pico64 (ab 0.11.1) und Pico USB Keyboard (ab 0.4.1) drücken dann das rechte statt des linken SHIFT; der Blue-64 (ab v1.12-ble2) drückt das rechte SHIFT nur, wenn keine andere Taste gehalten wird, sonst das linke (er kann nur eine Matrixposition ansprechen) |

`key` ist die Position in der C64-Tastaturmatrix `PA * 8 + PB` (0–63); PA ist die Spaltenleitung
(CIA1 Port A), PB die Zeilenleitung (CIA1 Port B). `0xFF` = keine Taste.
Beispiele: RETURN = `0*8+1 = 1`, A = `1*8+2 = 10`, SPACE = `7*8+4 = 60`.

Jeder Schreibvorgang beschreibt den vollständigen Tastaturzustand. `[0x00, 0xFF]` lässt alles los.

## Matrix (Erweiterung)

Neun Bytes: `[flags, col0, col1, …, col7]` – `flags` wie oben, `colN` ist eine Bitmaske der
gedrückten PB-Zeilen in PA-Spalte N. Damit gehen beliebig viele Tasten gleichzeitig. Vor der
Nutzung das Fähigkeiten-Byte der Info-Characteristic prüfen.

## Text

In Stücken von höchstens (ATT-MTU − 3) Bytes schreiben:

- Byte 0: Bit 0 = erstes Stück, Bit 1 = letztes Stück
- Bytes 1…: Text in BT-64-Makrosyntax (Kleinbuchstaben werden ungeshiftet getippt,
  `~ret~`, `~clr~`, `~home~`, `~del~`, `~inst~`, `~f1~` … `~f8~`, `~up~`, `~dn~`, `~ll~`, `~rr~`,
  `~stop~`, `~run~`, `~pi~`, `~arup~`, `~arll~`, gehaltene Umschalttasten `~shft-psh~` / `~shft-rel~` usw.)

Gesamtlänge höchstens 1024 Bytes. Solange noch ein vorheriger Text getippt wird, wird das letzte
Stück mit ATT-Fehler `0x80` (busy) abgelehnt – kurz warten und erneut senden. ATT-Fehler `0x81`
heißt: Text zu lang. Tasten-Schreibvorgänge werden ignoriert, solange ein Text getippt wird.

## Info

Lesen liefert `[Protokollversion, Status, Fähigkeiten]`:

- Protokollversion: `1`
- Status Bit 0: Text wird getippt
- Fähigkeiten Bit 0: Matrix-Characteristic vorhanden *(nur Pico64; der BT-64 liefert zwei Bytes)*

## Zeitverhalten

- Jeder Tastenzustand mit gedrückter Taste wird mindestens **60 ms** gehalten, ein Loslassen
  **30 ms**, Schritte beim Texttippen **40 ms** – das KERNAL fragt die Tastatur alle 1/60 s ab, so
  wird auch der kürzeste Tipper erkannt. Zustände kommen in eine Warteschlange (32 Einträge),
  nichts geht verloren.
- Nach dem Verbinden bittet der Pico um ein Verbindungsintervall von 15–30 ms (innerhalb der
  Apple-Richtlinien für Zubehör), damit Tastendrücke auch unter iOS schnell ankommen.
- Trennt sich der Client, werden alle Tasten losgelassen; das Advertising startet von selbst neu.

# Kabeleingang (CBM2 Keyboard)

Ab Firmware 0.13.1 lässt sich das [CBM2 Keyboard](https://github.com/do2mad/cbm2-keyboard) auch per
Kabel anschließen (Buchse J3 auf der Platine V2, Verdrahtung in [HARDWARE.de.md](HARDWARE.de.md)).

- **UART:** GP17 (Pin 22) = UART0 RX mit Pull-up, **115200 Baud 8N1**. Ohne Kabel passiert nichts.
- **Rahmen** (11 Bytes): `0xC6 0x4B flags col0 … col7 check`
  - `flags`, `colN`: wie bei der Matrix-Characteristic (Bit 3 von `flags` = RESTORE)
  - `check` = XOR von `flags` und `col0` … `col7`, XOR `0x5A`
- Der Sender schickt bei jeder Änderung und wiederholt den Zustand alle **100 ms**. Kommt
  **300 ms** lang kein gültiger Rahmen, werden die Tasten dieser Quelle losgelassen.
- Die Tasten gehen **direkt an die Matrix** – ohne Warteschlange und ohne Mindesthaltezeiten, wie
  bei einer echten Tastatur. Verzögerung etwa 3–5 ms, schnell genug für Spiele.
