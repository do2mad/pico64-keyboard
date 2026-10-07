// Pico64 Keyboard – Bluetooth (HID) keyboard to C64 matrix mapping
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// Two modes (F10 switches):
//
// * Symbolic (German QWERTZ, default): what is printed on the key arrives on
//   the C64. Shift+0 ("=") becomes the C64 "=" key WITHOUT shift, Shift+2 (")
//   becomes C64 Shift+2, AltGr+Q ("@") the C64 "@" key, and so on. Characters
//   the C64 doesn't have (ä ö ü ß § ° € ´ { } | \ ~) are ignored.
// * Positional: every PC key acts like the C64 key at the same place (row by
//   row, US/ISO layout), SHIFT stays SHIFT. Best for games.
//
// Keys that work the same in both modes:
//   Esc = RUN/STOP, Tab = CTRL, Backspace/Del = INST/DEL, Home = CLR/HOME,
//   Insert = £, F1..F8 = F1..F8, F9 / Page Up / Pause = RESTORE,
//   cursor keys = CRSR (up/left with SHIFT), Ctrl = CTRL, Alt / Windows = C=.

#include "keymap.h"

#include <stddef.h>

// C64 matrix codes (PA * 8 + PB)
enum {
    C_DEL = 0x00, C_RETURN = 0x01, C_CRSR_LR = 0x02, C_F7 = 0x03, C_F1 = 0x04, C_F3 = 0x05, C_F5 = 0x06, C_CRSR_UD = 0x07,
    C_3 = 0x08, C_W = 0x09, C_A = 0x0A, C_4 = 0x0B, C_Z = 0x0C, C_S = 0x0D, C_E = 0x0E, C_LSHIFT = 0x0F,
    C_5 = 0x10, C_R = 0x11, C_D = 0x12, C_6 = 0x13, C_C = 0x14, C_F = 0x15, C_T = 0x16, C_X = 0x17,
    C_7 = 0x18, C_Y = 0x19, C_G = 0x1A, C_8 = 0x1B, C_B = 0x1C, C_H = 0x1D, C_U = 0x1E, C_V = 0x1F,
    C_9 = 0x20, C_I = 0x21, C_J = 0x22, C_0 = 0x23, C_M = 0x24, C_K = 0x25, C_O = 0x26, C_N = 0x27,
    C_PLUS = 0x28, C_P = 0x29, C_L = 0x2A, C_MINUS = 0x2B, C_PERIOD = 0x2C, C_COLON = 0x2D, C_AT = 0x2E, C_COMMA = 0x2F,
    C_POUND = 0x30, C_STAR = 0x31, C_SEMI = 0x32, C_HOME = 0x33, C_RSHIFT = 0x34, C_EQUAL = 0x35, C_UPARROW = 0x36, C_SLASH = 0x37,
    C_1 = 0x38, C_LEFTARROW = 0x39, C_CTRL = 0x3A, C_2 = 0x3B, C_SPACE = 0x3C, C_CMDR = 0x3D, C_Q = 0x3E, C_STOP = 0x3F,
    C_NONE = 0xFF,
    C_RESTORE = 0xFE,       // not in the matrix: own line
};

// How a mapped key treats SHIFT
enum { SH_KEEP = 0, SH_ON = 1, SH_OFF = 2 };

typedef struct {
    uint8_t code;
    uint8_t shift;
} c64key_t;

static const c64key_t NONE = { C_NONE, SH_KEEP };

static c64key_t K(uint8_t code, uint8_t shift) { c64key_t k = { code, shift }; return k; }

// HID usage 0x04..0x1D = A..Z (US positions)
static const uint8_t letters[26] = {
    C_A, C_B, C_C, C_D, C_E, C_F, C_G, C_H, C_I, C_J, C_K, C_L, C_M,
    C_N, C_O, C_P, C_Q, C_R, C_S, C_T, C_U, C_V, C_W, C_X, C_Y, C_Z,
};
// HID usage 0x1E..0x27 = 1..9, 0
static const uint8_t digits[10] = { C_1, C_2, C_3, C_4, C_5, C_6, C_7, C_8, C_9, C_0 };

// Keys that are the same in both modes
static c64key_t common_key(uint8_t u) {
    switch (u) {
    case 0x28: case 0x58: return K(C_RETURN, SH_KEEP);   // Enter, keypad Enter
    case 0x29: return K(C_STOP, SH_KEEP);                  // Esc
    case 0x2A: return K(C_DEL, SH_KEEP);                   // Backspace
    case 0x2B: return K(C_CTRL, SH_KEEP);                  // Tab (same place as C64 CTRL)
    case 0x2C: return K(C_SPACE, SH_KEEP);
    case 0x3A: return K(C_F1, SH_OFF);                     // F1..F8
    case 0x3B: return K(C_F1, SH_ON);
    case 0x3C: return K(C_F3, SH_OFF);
    case 0x3D: return K(C_F3, SH_ON);
    case 0x3E: return K(C_F5, SH_OFF);
    case 0x3F: return K(C_F5, SH_ON);
    case 0x40: return K(C_F7, SH_OFF);
    case 0x41: return K(C_F7, SH_ON);
    case 0x42: case 0x4B: case 0x48: return K(C_RESTORE, SH_KEEP);   // F9, Page Up, Pause
    case 0x49: return K(C_POUND, SH_KEEP);                 // Insert
    case 0x4A: return K(C_HOME, SH_KEEP);                  // Home (+Shift = CLR)
    case 0x4C: return K(C_DEL, SH_KEEP);                   // Delete
    case 0x4F: return K(C_CRSR_LR, SH_OFF);                // Right
    case 0x50: return K(C_CRSR_LR, SH_ON);                 // Left
    case 0x51: return K(C_CRSR_UD, SH_OFF);                // Down
    case 0x52: return K(C_CRSR_UD, SH_ON);                 // Up
    case 0x54: return K(C_SLASH, SH_OFF);                  // keypad / * - + .
    case 0x55: return K(C_STAR, SH_OFF);
    case 0x56: return K(C_MINUS, SH_OFF);
    case 0x57: return K(C_PLUS, SH_OFF);
    case 0x63: return K(C_PERIOD, SH_OFF);
    default:
        if (u >= 0x59 && u <= 0x62) return K(digits[u - 0x59], SH_OFF);   // keypad 1..9, 0
        return NONE;
    }
}

static c64key_t positional_key(uint8_t u) {
    if (u >= 0x04 && u <= 0x1D) return K(letters[u - 0x04], SH_KEEP);
    if (u >= 0x1E && u <= 0x27) return K(digits[u - 0x1E], SH_KEEP);
    switch (u) {
    case 0x2D: return K(C_PLUS, SH_KEEP);       // - _   -> +   (right of 0)
    case 0x2E: return K(C_MINUS, SH_KEEP);      // = +   -> -
    case 0x2F: return K(C_AT, SH_KEEP);         // [     -> @   (right of P)
    case 0x30: return K(C_STAR, SH_KEEP);       // ]     -> *
    case 0x31: return K(C_UPARROW, SH_KEEP);    // \     -> ↑
    case 0x32: return K(C_EQUAL, SH_KEEP);      // ISO # (left of Enter) -> =
    case 0x33: return K(C_COLON, SH_KEEP);      // ;     -> :   (right of L)
    case 0x34: return K(C_SEMI, SH_KEEP);       // '     -> ;
    case 0x35: return K(C_LEFTARROW, SH_KEEP);  // `     -> ←   (left of 1)
    case 0x36: return K(C_COMMA, SH_KEEP);
    case 0x37: return K(C_PERIOD, SH_KEEP);
    case 0x38: return K(C_SLASH, SH_KEEP);
    default:   return common_key(u);
    }
}

// German QWERTZ: usage -> character -> C64 key
static c64key_t symbolic_de_key(uint8_t u, bool shift, bool altgr) {
    if (u >= 0x04 && u <= 0x1D) {
        if (altgr) return (u == 0x14) ? K(C_AT, SH_OFF) : NONE;     // AltGr+Q = @
        uint8_t l = u - 0x04;
        if (u == 0x1C) l = 'Z' - 'A';                               // German: Y and Z swapped
        else if (u == 0x1D) l = 'Y' - 'A';
        return K(letters[l], SH_KEEP);
    }
    if (u >= 0x1E && u <= 0x27) {
        int d = u - 0x1E;                                           // 0 = "1" ... 9 = "0"
        if (altgr) {
            if (d == 7) return K(C_COLON, SH_ON);                   // AltGr+8 = [
            if (d == 8) return K(C_SEMI, SH_ON);                    // AltGr+9 = ]
            return NONE;                                            // { } ² ³
        }
        if (!shift) return K(digits[d], SH_OFF);
        switch (d) {
        case 0: return K(C_1, SH_ON);        // !
        case 1: return K(C_2, SH_ON);        // "
        case 2: return NONE;                 // §
        case 3: return K(C_4, SH_ON);        // $
        case 4: return K(C_5, SH_ON);        // %
        case 5: return K(C_6, SH_ON);        // &
        case 6: return K(C_SLASH, SH_OFF);   // /
        case 7: return K(C_8, SH_ON);        // (
        case 8: return K(C_9, SH_ON);        // )
        default: return K(C_EQUAL, SH_OFF);  // =
        }
    }
    if (altgr) return NONE;                  // | \ ~ € µ ...
    switch (u) {
    case 0x2D: return shift ? K(C_SLASH, SH_ON) : NONE;                       // ß ?
    case 0x2E: return NONE;                                                   // ´ `
    case 0x2F: return NONE;                                                   // ü
    case 0x30: return shift ? K(C_STAR, SH_OFF) : K(C_PLUS, SH_OFF);          // + *
    case 0x31: case 0x32:
               return shift ? K(C_7, SH_ON) : K(C_3, SH_ON);                  // # '
    case 0x33: case 0x34: return NONE;                                        // ö ä
    case 0x35: return shift ? NONE : K(C_UPARROW, SH_OFF);                    // ^ °
    case 0x36: return shift ? K(C_SEMI, SH_OFF) : K(C_COMMA, SH_OFF);         // , ;
    case 0x37: return shift ? K(C_COLON, SH_OFF) : K(C_PERIOD, SH_OFF);       // . :
    case 0x38: return shift ? K(C_LEFTARROW, SH_OFF) : K(C_MINUS, SH_OFF);    // - _
    case 0x64: return shift ? K(C_PERIOD, SH_ON) : K(C_COMMA, SH_ON);         // < >
    default:   return common_key(u);
    }
}

void keymap_build(keymap_mode_t mode, uint8_t mods, const uint8_t *keys, int n, kb_state_t *out) {
    kb_state_t s = {0};
    bool pc_shift = (mods & (HID_MOD_LSHIFT | HID_MOD_RSHIFT)) != 0;
    bool altgr = (mode == KEYMAP_SYMBOLIC_DE) && (mods & HID_MOD_RALT);
    int shift_override = SH_KEEP;            // the last pressed key decides

    for (int i = 0; i < n; i++) {
        uint8_t u = keys[i];
        if (u < 0x04 || u == HID_KEY_F10) continue;     // empty / error codes / mode switch
        c64key_t k = (mode == KEYMAP_POSITIONAL) ? positional_key(u) : symbolic_de_key(u, pc_shift, altgr);
        if (k.code == C_NONE) continue;
        if (k.code == C_RESTORE) { s.restore = true; continue; }
        kb_press_code(&s, k.code);
        if (k.shift != SH_KEEP) shift_override = k.shift;
    }

    // Modifiers
    if (mods & (HID_MOD_LCTRL | HID_MOD_RCTRL)) kb_press(&s, KB_CTRL_COL, KB_CTRL_ROW);
    uint8_t cmdr_mask = HID_MOD_LALT | HID_MOD_LGUI | HID_MOD_RGUI;
    if (mode == KEYMAP_POSITIONAL) cmdr_mask |= HID_MOD_RALT;   // AltGr is a layer in symbolic mode
    if (mods & cmdr_mask) kb_press(&s, KB_CMDR_COL, KB_CMDR_ROW);

    if (mode == KEYMAP_POSITIONAL) {
        if (mods & HID_MOD_LSHIFT) kb_press(&s, KB_LSHIFT_COL, KB_LSHIFT_ROW);
        if (mods & HID_MOD_RSHIFT) kb_press(&s, KB_RSHIFT_COL, KB_RSHIFT_ROW);
        // Shift stays what it is (Shift+F1 = F2 like on the C64); only keys
        // that need SHIFT on the C64 (F2, CRSR up/left) add it.
        if (shift_override == SH_ON) kb_press(&s, KB_LSHIFT_COL, KB_LSHIFT_ROW);
    } else {
        bool shift = pc_shift;
        if (shift_override == SH_ON) shift = true;
        else if (shift_override == SH_OFF) shift = false;
        if (shift) kb_press(&s, KB_LSHIFT_COL, KB_LSHIFT_ROW);
    }
    *out = s;
}

const char *keymap_mode_name(keymap_mode_t mode) {
    return mode == KEYMAP_POSITIONAL ? "positional" : "symbolic (German QWERTZ)";
}
