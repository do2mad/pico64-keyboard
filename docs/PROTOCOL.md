# Protocols

*Deutsche Version: [PROTOCOL.de.md](PROTOCOL.de.md)*

# Bluetooth LE protocol

Pico64 Keyboard implements the **BT-64 BLE keyboard protocol v1** (the same service the
[Blue-64 Keyboard app](https://github.com/do2mad/blue64-keyboard-ios) uses with a BT-64 / Blue-64)
plus one extension for full-matrix states.

The Pico advertises as **`Pico64`** with the 128-bit service UUID
**`C64B0001-B1E6-4A64-9C64-6B7E3F1A2D00`**. No pairing is required.

| Characteristic | UUID | Properties |
|---|---|---|
| Key | `C64B0002-B1E6-4A64-9C64-6B7E3F1A2D00` | write, write without response |
| Text | `C64B0003-B1E6-4A64-9C64-6B7E3F1A2D00` | write |
| Info | `C64B0004-B1E6-4A64-9C64-6B7E3F1A2D00` | read |
| Matrix *(Pico64 extension)* | `C64B0005-B1E6-4A64-9C64-6B7E3F1A2D00` | write, write without response |

## Key

Two bytes: `[flags, key]`

| flags bit | meaning |
|---|---|
| 0 | SHIFT (left SHIFT) |
| 1 | C= (Commodore key) |
| 2 | CTRL |
| 3 | RESTORE |
| 4 | right SHIFT – only together with bit 0. Pico64 (≥ 0.11.1) and Pico USB Keyboard (≥ 0.4.1) then press the right SHIFT key instead of the left one; the Blue-64 (from v1.12-ble2) presses the right SHIFT only when no other key is held, otherwise the left one (it can address only one matrix position) |

`key` is the C64 keyboard matrix position `PA * 8 + PB` (0–63), where PA is the column line
(CIA1 port A) and PB the row line (CIA1 port B). `0xFF` = no key.
Examples: RETURN = `0*8+1 = 1`, A = `1*8+2 = 10`, SPACE = `7*8+4 = 60`.

Every write describes the complete keyboard state. `[0x00, 0xFF]` releases everything.

## Matrix (extension)

Nine bytes: `[flags, col0, col1, …, col7]` – `flags` as above, `colN` is a bit mask of the pressed
PB rows in PA column N. This allows any number of keys at the same time. Check the capabilities
byte of the info characteristic before using it.

## Text

Write in chunks of at most (ATT MTU − 3) bytes:

- byte 0: bit 0 = first chunk, bit 1 = last chunk
- bytes 1…: text in BT-64 macro syntax (lower-case letters are typed unshifted,
  `~ret~`, `~clr~`, `~home~`, `~del~`, `~inst~`, `~f1~` … `~f8~`, `~up~`, `~dn~`, `~ll~`, `~rr~`,
  `~stop~`, `~run~`, `~pi~`, `~arup~`, `~arll~`, held modifiers `~shft-psh~` / `~shft-rel~` etc.)

Maximum total length: 1024 bytes. While a previous text is still being typed, the last chunk is
rejected with ATT error `0x80` (busy) – retry after a short delay. ATT error `0x81` means the
text is too long. Key writes are ignored while a text is being typed.

## Info

Read returns `[protocol version, status, capabilities]`:

- protocol version: `1`
- status bit 0: text feed running
- capabilities bit 0: matrix characteristic available *(Pico64 only; the BT-64 returns two bytes)*

## Timing

- Every key state with a key pressed is held for at least **60 ms**, a release for **30 ms**,
  text feed steps for **40 ms** – the KERNAL scans the keyboard every 1/60 s, so even the
  shortest tap is seen. States are queued (32 entries), nothing is lost.
- After connecting, the Pico asks for a connection interval of 15–30 ms (within Apple's
  accessory guidelines), so key presses arrive quickly on iOS as well.
- All keys are released when the client disconnects; advertising restarts automatically.

# Cable input (CBM2 Keyboard)

From firmware 0.13.1 the [CBM2 Keyboard](https://github.com/do2mad/cbm2-keyboard) can also be
connected by cable (socket J3 on the V2 PCB, wiring in [HARDWARE.md](HARDWARE.md)).

- **UART:** GP17 (pin 22) = UART0 RX with pull-up, **115200 baud 8N1**. Without a cable nothing happens.
- **Frame** (11 bytes): `0xC6 0x4B flags col0 … col7 check`
  - `flags`, `colN`: as in the matrix characteristic (bit 3 of `flags` = RESTORE)
  - `check` = XOR of `flags` and `col0` … `col7`, XOR `0x5A`
- The sender sends on every change and repeats the state every **100 ms**. Without a valid frame for
  **300 ms** the keys of this source are released.
- The keys go **straight to the matrix** – no queue and no minimum hold times, like a real
  keyboard. Latency is about 3–5 ms, fast enough for games.
