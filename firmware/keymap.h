// Pico64 Keyboard – Bluetooth (HID) keyboard to C64 matrix mapping
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
#ifndef PICO64_KEYMAP_H
#define PICO64_KEYMAP_H

#include <stdint.h>
#include <stdbool.h>

#include "matrix.h"

typedef enum {
    KEYMAP_SYMBOLIC_DE = 0,   // the character printed on a German keyboard is typed ("=" is "=")
    KEYMAP_POSITIONAL  = 1,   // the key at the same place as on the C64 keyboard (games)
} keymap_mode_t;

// HID modifier bits (same as the HID boot protocol / Bluepad32)
#define HID_MOD_LCTRL  0x01
#define HID_MOD_LSHIFT 0x02
#define HID_MOD_LALT   0x04
#define HID_MOD_LGUI   0x08
#define HID_MOD_RCTRL  0x10
#define HID_MOD_RSHIFT 0x20
#define HID_MOD_RALT   0x40   // AltGr on German keyboards
#define HID_MOD_RGUI   0x80

#define HID_KEY_F10    0x43   // switches the mapping mode

/**
 * Builds the C64 keyboard state for the keys currently held on the Bluetooth
 * keyboard. `keys` are HID usage codes (0 = empty slot), in the order they
 * were pressed.
 */
void keymap_build(keymap_mode_t mode, uint8_t modifiers, const uint8_t *keys, int n, kb_state_t *out);

const char *keymap_mode_name(keymap_mode_t mode);

#endif
