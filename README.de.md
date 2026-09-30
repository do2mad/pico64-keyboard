# Pico64 Keyboard

*English version: [README.md](README.md)*

Ein kleiner Adapter, der aus einem **Raspberry Pi Pico 2 W** eine kabellose Tastatur für den
**Commodore 64** macht – bedient vom Handy mit der
[Blue-64-Keyboard-App](https://github.com/do2mad/blue64-keyboard-ios) (iPhone und Android).

Der Pico wird zwischen C64-Mainboard und Tastaturkabel gesteckt und bildet die Tastaturmatrix in
Software nach. Außer dem Pico braucht es nur **17 Widerstände und eine Diode** – keine
Pegelwandler und kein Koppelfeld-Chip, denn GPIO 0–25 des RP2350 sind 5-V-tolerant.
Originaltastatur und Joysticks funktionieren weiter.

![Verdrahtung](docs/wiring.png)

## Funktionen

- Bildet die komplette 8×8-Tastaturmatrix plus RESTORE nach – beliebig viele Tasten gleichzeitig
- Reagiert in etwa 70 ns auf jede Spalte, die der C64 auswählt (Kern 1 arbeitet mit einer
  Tabelle aus dem RAM) – auch Programme, die mehrere Spalten gleichzeitig abfragen, funktionieren
- Derselbe Bluetooth-LE-Dienst wie die BT-64-/Blue-64-Firmware mit BLE-Tastaturdienst –
  die Blue-64-Keyboard-App funktioniert unverändert (der Pico heißt dort **Pico64**)
- Texte und kurze BASIC-Listings aus der App tippen lassen (Textbausteine)
- USB-Konsole: jede im Terminal eingegebene Zeile wird auf dem C64 getippt, dazu ein Tasten-Protokoll zur Fehlersuche
- Stromversorgung aus dem C64

## Stand

**v0.9.0 – funktionierender Prototyp.** Auf Lochraster aufgebaut und an einem Brotkasten-C64 mit
iPhone und Android-Handy getestet. Eine KiCad-Platine ist in Arbeit (siehe [hardware/](hardware/)).

## Loslegen

1. **Adapter bauen** – Verdrahtung, Stückliste, Sicherheitshinweise: [docs/HARDWARE.md](docs/HARDWARE.md) (englisch).
   Die Belegung von CN1 am eigenen C64 vor dem Löten mit dem Multimeter prüfen.
2. **Firmware aufspielen** – BOOTSEL gedrückt halten, Pico per USB an den Rechner, die Datei
   `pico64-keyboard-vX.Y.Z.uf2` aus dem [neuesten Release](https://github.com/do2mad/pico64-keyboard/releases/latest)
   auf das Laufwerk `RP2350` ziehen. Dabei den Pico **nicht** im laufenden C64 stecken lassen.
3. **Einstecken** zwischen CN1 und Tastaturkabel, C64 einschalten – die LED am Pico blinkt
   (wartet auf die App).
4. **Mit der App verbinden** – [Blue-64 Keyboard](https://github.com/do2mad/blue64-keyboard-ios)
   findet den Pico selbst, die LED leuchtet dauerhaft, solange verbunden.

Firmware selbst bauen: [docs/BUILDING.md](docs/BUILDING.md). Bluetooth-Protokoll: [docs/PROTOCOL.md](docs/PROTOCOL.md).

## USB-Konsole

Pico per USB anschließen, während er im C64 steckt (die Diode schützt beide Seiten), und ein
Terminal mit 115200 Baud öffnen, z. B. auf dem Mac `screen /dev/tty.usbmodem* 115200`.

- Jede eingegebene Zeile wird auf dem C64 getippt, danach RETURN (Buchstaben ungeshiftet,
  `~clr~`, `~home~`, `~f1~` usw. funktionieren).
- Eine Zeile nur mit `d` schaltet das Tasten-Protokoll an/aus: jeder per Bluetooth empfangene
  Tastenzustand und jeder an die Matrix angelegte Zustand mit Zeitstempel, dazu das Verbindungsintervall.

## Sicherheit

- Der Pico **treibt nie eine C64-Leitung auf High** – PB0–PB7 und RESTORE werden nur nach Masse
  gezogen oder losgelassen (Open-Drain), PA0–PA7 werden nur gelesen.
- Nur GP0–GP22 verwenden. GP26–GP28 sind **nicht** 5-V-tolerant, GP23–25 und GP29 gehören dem Funkchip.
- Die 5-V-Toleranz gilt nur, solange der Pico versorgt ist – durch die Speisung aus dem C64 ist das immer der Fall.
- Nachbau auf eigene Gefahr. Verdrahtung doppelt prüfen.

## Dank und Lizenz

- Firmware © 2026 Martin Oswald (do2mad) – [1mhz.de](https://1mhz.de), [MIT-Lizenz](LICENSE)
- Hardware (Schaltplan, Platine) © 2026 Martin Oswald, [CC BY-NC-SA 4.0](LICENSE-HARDWARE.md) – nicht-kommerziell
- Nutzt Raspberry Pi Pico SDK, BTstack und den CYW43-Treiber – siehe [NOTICE](NOTICE)
- Der Bluetooth-Dienst folgt dem Protokoll der BLE-Tastatur-Erweiterung für
  [BT-64 / Blue-64](https://github.com/sideprojectslab/BT-64) von SideProjectsLab
- Belegung des C64-Tastatursteckers nach Ruud Baltissen

Änderungen: [CHANGELOG.md](CHANGELOG.md)

Commodore und Commodore 64 sind Marken ihrer jeweiligen Inhaber. Dieses Projekt steht in keiner
Verbindung zu ihnen, zu Raspberry Pi Ltd. oder zu SideProjectsLab.
