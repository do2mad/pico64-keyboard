// Pico64 Keyboard – USB keyboard mode: the real C64 keyboard as USB keyboard
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// Without a running C64 (powered by USB, see main.c) core 1 scans the real C64
// keyboard (matrix.c). Here its keys - plus keys from the app and Bluetooth
// keyboards - become USB key presses. Output modes, switched with RESTORE + F key
// (stored in flash):
//
//   RESTORE + F1   MiSTer   C64 core of the MiSTer (keys as the core expects them,
//                           same table as the Pico USB Keyboard); RESTORE alone = F11
//   RESTORE + F3   PC       German layout (Windows / Linux / Raspberry Pi)
//   RESTORE + F5   PC       US layout
//   RESTORE + F7   Mac      German layout
//   RESTORE + F8   Mac      US layout (SHIFT + F7)
//
// PC / Mac: types what is printed on the C64 key (SHIFT + 2 = ", SHIFT + : = [),
// DEL = Backspace, INST = Insert, CRSR = cursor keys, F1..F8, HOME / CLR = Home / End,
// RUN/STOP = Esc, <- = Tab, CTRL = Ctrl, C= = Win / Cmd, pound = \ (SHIFT: |).
//
// Runs in the BTstack context (core 0). Also calls usb_kbd_task() in C64 mode
// (serial console).

#include "usb_mode.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "btstack.h"
#include "btstack_tlv.h"
#include "hardware/watchdog.h"
#include "pico/time.h"

#include "matrix.h"
#include "usb_kbd.h"

#define TICK_MS       1
#define F11_TAP_MS    80
#define TLV_TAG       0x50363455u      // 'P64U'

enum { OUT_MISTER = 0, OUT_PC_DE, OUT_PC_US, OUT_MAC_DE, OUT_MAC_US, OUT_COUNT };
static const char *const out_name[OUT_COUNT] = {
    "MiSTer C64 core", "PC, German layout", "PC, US layout", "Mac, German layout", "Mac, US layout",
};

#define PC_LAYOUT_US      0
#define PC_LAYOUT_DE      1
#define PC_LAYOUT_DE_MAC  2
#define PC_LAYOUT_US_MAC  3
static const uint8_t out_layout[OUT_COUNT] = { 0, PC_LAYOUT_DE, PC_LAYOUT_US, PC_LAYOUT_DE_MAC, PC_LAYOUT_US_MAC };

#define LCTRL  0x01
#define LSHIFT 0x02
#define LALT   0x04
#define LGUI   0x08
#define RSHIFT_MOD 0x20
#define RALT   0x40

static btstack_timer_source_t tick;
static bool    usb_active;              // USB keyboard mode
static uint8_t out_mode = OUT_MISTER;

// ---------------------------------------------------------------------------
// MiSTer: C64 key -> PC key the C64 core expects (from the Pico USB Keyboard)

// USB HID usage IDs (keyboard page)
enum {
    U_A = 0x04, U_1 = 0x1E, U_6 = 0x23, U_7 = 0x24, U_8 = 0x25, U_9 = 0x26, U_0 = 0x27,
    U_ENTER = 0x28, U_ESC = 0x29, U_BACKSPACE = 0x2A, U_TAB = 0x2B, U_SPACE = 0x2C,
    U_MINUS = 0x2D, U_EQUAL = 0x2E, U_LBRACE = 0x2F, U_RBRACE = 0x30, U_BACKSLASH = 0x31,
    U_SEMICOLON = 0x33, U_APOSTROPHE = 0x34, U_GRAVE = 0x35, U_COMMA = 0x36, U_DOT = 0x37,
    U_SLASH = 0x38, U_F1 = 0x3A, U_F3 = 0x3C, U_F5 = 0x3E, U_F7 = 0x40, U_F9 = 0x42,
    U_F11 = 0x44, U_HOME = 0x4A, U_END = 0x4D, U_RIGHT = 0x4F, U_DOWN = 0x51,
    U_KP1 = 0x59, U_KP2 = 0x5A, U_KP3 = 0x5B, U_KP4 = 0x5C, U_KP5 = 0x5D,
    // pseudo codes for modifiers (become bits in the report)
    M_LCTRL = 0xE0, M_LSHIFT = 0xE1, M_RSHIFT = 0xE5,
};
#define L(c) (U_A + ((c) - 'A'))

// C64 matrix code (PA*8 + PB) -> HID usage
static const uint8_t matrix_usage[64] = {
    // PA0: DEL RETURN CRSR-LR F7 F1 F3 F5 CRSR-UD
    U_BACKSPACE, U_ENTER, U_RIGHT, U_F7, U_F1, U_F3, U_F5, U_DOWN,
    // PA1: 3 W A 4 Z S E LSHIFT
    U_KP3, L('W'), L('A'), U_KP4, L('Z'), L('S'), L('E'), M_LSHIFT,
    // PA2: 5 R D 6 C F T X
    U_KP5, L('R'), L('D'), U_6, L('C'), L('F'), L('T'), L('X'),
    // PA3: 7 Y G 8 B H U V
    U_7, L('Y'), L('G'), U_8, L('B'), L('H'), L('U'), L('V'),
    // PA4: 9 I J 0 M K O N
    U_9, L('I'), L('J'), U_0, L('M'), L('K'), L('O'), L('N'),
    // PA5: + P L - . : @ ,
    U_EQUAL, L('P'), L('L'), U_MINUS, U_DOT, U_SEMICOLON, U_LBRACE, U_COMMA,
    // PA6: £ * ; HOME RSHIFT = ↑ /
    U_BACKSLASH, U_RBRACE, U_APOSTROPHE, U_HOME, M_RSHIFT, U_END, U_F9, U_SLASH,
    // PA7: 1 ← CTRL 2 SPACE C= Q RUN/STOP
    U_KP1, U_GRAVE, M_LCTRL, U_KP2, U_SPACE, U_TAB, L('Q'), U_ESC,
};

// With SHIFT: C64 6 -> PC 7, 7 -> 6, 8 -> 9, 9 -> 0, 0 -> nothing (see above)
static uint8_t shifted_digit(uint8_t code) {
    switch (code) {
    case 0x13: return U_7;
    case 0x18: return U_6;
    case 0x1B: return U_9;
    case 0x20: return U_0;
    case 0x23: return 0;
    default:   return 0xFF;      // not a remapped digit
    }
}


static void map_mister(const kb_state_t *s, uint8_t *mods, uint8_t keys[6]) {
    uint8_t mod = 0;
    int n = 0;
    bool shift = (s->cols[KB_LSHIFT_COL] & (1u << KB_LSHIFT_ROW)) ||
                 (s->cols[KB_RSHIFT_COL] & (1u << KB_RSHIFT_ROW));
    for (uint8_t code = 0; code < 64; code++) {
        if (!(s->cols[code >> 3] & (1u << (code & 7)))) continue;
        uint8_t u = matrix_usage[code];
        if (shift) {
            uint8_t d = shifted_digit(code);
            if (d != 0xFF) u = d;
        }
        if (u == 0) continue;
        if (u >= 0xE0) { mod |= (uint8_t)(1u << (u - 0xE0)); continue; }
        if (n < 6) keys[n++] = u;
    }
    *mods = mod;
}

// ---------------------------------------------------------------------------
// PC / Mac: character printed on the C64 key -> key of the PC layout

typedef struct { uint8_t mods, key; } stroke_t;

#define S(m, k) ((stroke_t){ (m), (k) })
#define NONE    ((stroke_t){ 0, 0 })
#define SPACE   0x2C

static stroke_t letter(uint32_t c, uint8_t layout) {
    bool upper = c >= 'A' && c <= 'Z';
    uint32_t l = upper ? c - 'A' + 'a' : c;
    uint8_t key = (uint8_t)(0x04 + (l - 'a'));
    if (layout == PC_LAYOUT_DE || layout == PC_LAYOUT_DE_MAC) {   // German: Y and Z swapped
        if (l == 'y') key = 0x1D;
        else if (l == 'z') key = 0x1C;
    }
    return S(upper ? LSHIFT : 0, key);
}

static stroke_t digit(uint32_t c) {
    return S(0, c == '0' ? 0x27 : (uint8_t)(0x1E + (c - '1')));
}

// returns the number of strokes (0 = not typeable); dead keys get a SPACE after them
static int char_strokes(uint32_t c, uint8_t layout, stroke_t out[2]) {
    out[1] = NONE;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) { out[0] = letter(c, layout); return 1; }
    if (c >= '0' && c <= '9') { out[0] = digit(c); return 1; }
    switch (c) {
    case '\n': out[0] = S(0, 0x28); return 1;
    case '\t': out[0] = S(0, 0x2B); return 1;
    case ' ':  out[0] = S(0, SPACE); return 1;
    }

    if (layout == PC_LAYOUT_US_MAC) {
        // a few Option characters of the US Mac layout
        switch (c) {
        case 0x20AC: out[0] = S(LALT | LSHIFT, 0x1F); return 1;    // € Opt+Shift+2
        case 0xA3:   out[0] = S(LALT, 0x20); return 1;             // £ Opt+3
        case 0xB0:   out[0] = S(LALT | LSHIFT, 0x25); return 1;    // ° Opt+Shift+8
        }
    }
    if (layout == PC_LAYOUT_US || layout == PC_LAYOUT_US_MAC) {
        static const struct { char c; uint8_t mods, key; } us[] = {
            {'!',LSHIFT,0x1E},{'@',LSHIFT,0x1F},{'#',LSHIFT,0x20},{'$',LSHIFT,0x21},{'%',LSHIFT,0x22},
            {'^',LSHIFT,0x23},{'&',LSHIFT,0x24},{'*',LSHIFT,0x25},{'(',LSHIFT,0x26},{')',LSHIFT,0x27},
            {'-',0,0x2D},{'_',LSHIFT,0x2D},{'=',0,0x2E},{'+',LSHIFT,0x2E},{'[',0,0x2F},{'{',LSHIFT,0x2F},
            {']',0,0x30},{'}',LSHIFT,0x30},{'\\',0,0x31},{'|',LSHIFT,0x31},{';',0,0x33},{':',LSHIFT,0x33},
            {'\'',0,0x34},{'"',LSHIFT,0x34},{'`',0,0x35},{'~',LSHIFT,0x35},{',',0,0x36},{'<',LSHIFT,0x36},
            {'.',0,0x37},{'>',LSHIFT,0x37},{'/',0,0x38},{'?',LSHIFT,0x38},
        };
        for (size_t i = 0; i < sizeof(us) / sizeof(us[0]); i++)
            if ((uint32_t)(uint8_t)us[i].c == c) {
                uint8_t key = us[i].key;
                // macOS swaps the ` key (0x35) with the ISO key (0x64), see below
                if (layout == PC_LAYOUT_US_MAC && key == 0x35) key = 0x64;
                out[0] = S(us[i].mods, key);
                return 1;
            }
        return 0;
    }

    bool mac = layout == PC_LAYOUT_DE_MAC;
    uint8_t alt = mac ? LALT : RALT;        // Option (Mac) / AltGr (PC)
    stroke_t s = NONE;
    bool dead = false;
    switch (c) {
    // shifted digits
    case '!': s = S(LSHIFT, 0x1E); break;
    case '"': s = S(LSHIFT, 0x1F); break;
    case 0xA7: s = S(LSHIFT, 0x20); break;          // §
    case '$': s = S(LSHIFT, 0x21); break;
    case '%': s = S(LSHIFT, 0x22); break;
    case '&': s = S(LSHIFT, 0x23); break;
    case '/': s = S(LSHIFT, 0x24); break;
    case '(': s = S(LSHIFT, 0x25); break;
    case ')': s = S(LSHIFT, 0x26); break;
    case '=': s = S(LSHIFT, 0x27); break;
    // key right of 0: ß ? (\ on PC)
    case 0xDF: s = S(0, 0x2D); break;               // ß
    case '?': s = S(LSHIFT, 0x2D); break;
    // accent key (dead)
    case 0xB4: s = S(0, 0x2E); dead = true; break;  // ´
    case '`': s = S(LSHIFT, 0x2E); dead = true; break;
    // ü + * # '
    case 0xFC: s = S(0, 0x2F); break;               // ü
    case 0xDC: s = S(LSHIFT, 0x2F); break;          // Ü
    case '+': s = S(0, 0x30); break;
    case '*': s = S(LSHIFT, 0x30); break;
    case 0xF6: s = S(0, 0x33); break;               // ö
    case 0xD6: s = S(LSHIFT, 0x33); break;          // Ö
    case 0xE4: s = S(0, 0x34); break;               // ä
    case 0xC4: s = S(LSHIFT, 0x34); break;          // Ä
    case '#': s = S(0, 0x32); break;
    case '\'': s = S(LSHIFT, 0x32); break;
    // ^ key (dead)
    // macOS swaps the ^ key (0x35) and the < key (0x64) of ISO keyboards
    // when it does not know the keyboard type -> send them swapped on the Mac
    case '^': s = S(0, mac ? 0x64 : 0x35); dead = true; break;
    case 0xB0: s = S(LSHIFT, mac ? 0x64 : 0x35); break;          // °
    // , . - < >
    case ',': s = S(0, 0x36); break;
    case ';': s = S(LSHIFT, 0x36); break;
    case '.': s = S(0, 0x37); break;
    case ':': s = S(LSHIFT, 0x37); break;
    case '-': s = S(0, 0x38); break;
    case '_': s = S(LSHIFT, 0x38); break;
    case '<': s = S(0, mac ? 0x35 : 0x64); break;
    case '>': s = S(LSHIFT, mac ? 0x35 : 0x64); break;
    // AltGr (PC) / Option (Mac) characters
    case '@':  s = mac ? S(alt, 0x0F) : S(alt, 0x14); break;            // Opt+L / AltGr+Q
    case 0x20AC: s = S(alt, 0x08); break;                               // € Opt/AltGr+E
    case '[':  s = mac ? S(alt, 0x22) : S(alt, 0x25); break;            // Opt+5 / AltGr+8
    case ']':  s = mac ? S(alt, 0x23) : S(alt, 0x26); break;            // Opt+6 / AltGr+9
    case '{':  s = mac ? S(alt, 0x25) : S(alt, 0x24); break;            // Opt+8 / AltGr+7
    case '}':  s = mac ? S(alt, 0x26) : S(alt, 0x27); break;            // Opt+9 / AltGr+0
    case '|':  s = mac ? S(alt, 0x24) : S(alt, 0x64); break;            // Opt+7 / AltGr+<
    case '\\': s = mac ? S(alt | LSHIFT, 0x24) : S(alt, 0x2D); break;   // Opt+Shift+7 / AltGr+ß
    case '~':                                                            // Opt+N (dead) / AltGr++
        if (mac) { s = S(alt, 0x11); dead = true; } else s = S(alt, 0x30);
        break;
    default: return 0;
    }
    out[0] = s;
    if (dead) { out[1] = S(0, SPACE); return 2; }
    return 1;
}

// [unshifted, shifted] character of every C64 key, 0 = special key (below)
static const char pc_chr[64][2] = {
    // PA0: DEL RETURN CRSR-LR F7 F1 F3 F5 CRSR-UD
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    // PA1: 3 W A 4 Z S E LSHIFT
    {'3','#'},{'w','W'},{'a','A'},{'4','$'},{'z','Z'},{'s','S'},{'e','E'},{0,0},
    // PA2: 5 R D 6 C F T X
    {'5','%'},{'r','R'},{'d','D'},{'6','&'},{'c','C'},{'f','F'},{'t','T'},{'x','X'},
    // PA3: 7 Y G 8 B H U V
    {'7','\''},{'y','Y'},{'g','G'},{'8','('},{'b','B'},{'h','H'},{'u','U'},{'v','V'},
    // PA4: 9 I J 0 M K O N
    {'9',')'},{'i','I'},{'j','J'},{'0','0'},{'m','M'},{'k','K'},{'o','O'},{'n','N'},
    // PA5: + P L - . : @ ,
    {'+','+'},{'p','P'},{'l','L'},{'-','-'},{'.','>'},{':','['},{'@','@'},{',','<'},
    // PA6: pound * ; HOME RSHIFT = arrow-up /
    {'\\','|'},{'*','*'},{';',']'},{0,0},{0,0},{'=','='},{'^','^'},{'/','?'},
    // PA7: 1 <- CTRL 2 SPACE C= Q RUN/STOP
    {'1','!'},{0,0},{0,0},{'2','"'},{' ',' '},{0,0},{'q','Q'},{0,0},
};

// special keys: USB usage without / with SHIFT; keep = SHIFT is passed on
typedef struct { uint8_t code, plain, shifted; bool keep; } special_t;
static const special_t specials[] = {
    { 0, 0x2A, 0x49, false },   // DEL / INST -> Backspace / Insert
    { 1, 0x28, 0x28, true  },   // RETURN
    { 2, 0x4F, 0x50, false },   // CRSR right / left
    { 3, 0x40, 0x41, false },   // F7 / F8
    { 4, 0x3A, 0x3B, false },   // F1 / F2
    { 5, 0x3C, 0x3D, false },   // F3 / F4
    { 6, 0x3E, 0x3F, false },   // F5 / F6
    { 7, 0x51, 0x52, false },   // CRSR down / up
    {51, 0x4A, 0x4D, false },   // HOME / CLR -> Home / End
    {57, 0x2B, 0x2B, true  },   // <- -> Tab
    {63, 0x29, 0x29, false },   // RUN/STOP -> Esc
};

static void map_pc(const kb_state_t *s, uint8_t layout, uint8_t *mods, uint8_t keys[6]) {
    bool lsh = s->cols[KB_LSHIFT_COL] & (1u << KB_LSHIFT_ROW);
    bool rsh = s->cols[KB_RSHIFT_COL] & (1u << KB_RSHIFT_ROW);
    bool shift = lsh || rsh;
    uint8_t base = 0, char_mods = 0, keep_mods = 0;
    if (s->cols[KB_CTRL_COL] & (1u << KB_CTRL_ROW)) base |= LCTRL;
    if (s->cols[KB_CMDR_COL] & (1u << KB_CMDR_ROW)) base |= LGUI;
    uint8_t shift_mod = rsh ? RSHIFT_MOD : LSHIFT;
    int n = 0;

    for (uint8_t code = 0; code < 64 && n < 6; code++) {
        if (!(s->cols[code >> 3] & (1u << (code & 7)))) continue;
        char c = pc_chr[code][shift ? 1 : 0];
        if (c) {
            stroke_t st[2];
            if (!char_strokes((uint8_t)c, layout, st)) {
                c = pc_chr[code][0];                       // fall back to the unshifted character
                if (!char_strokes((uint8_t)c, layout, st)) continue;
            }
            keys[n++] = st[0].key;
            char_mods = st[0].mods;                        // the last character key wins
            continue;
        }
        for (size_t i = 0; i < sizeof(specials) / sizeof(specials[0]); i++) {
            if (specials[i].code != code) continue;
            keys[n++] = shift ? specials[i].shifted : specials[i].plain;
            if (specials[i].keep && shift) keep_mods |= shift_mod;
        }
    }
    // SHIFT alone (or with CTRL / C=) is sent as SHIFT, e.g. for shift-click
    if (n == 0 && shift) keep_mods |= shift_mod;
    *mods = base | char_mods | keep_mods;
}

// ---------------------------------------------------------------------------
// Output mode, RESTORE combinations

static void load_mode(void) {
    const btstack_tlv_t *tlv; void *ctx;
    btstack_tlv_get_instance(&tlv, &ctx);
    uint8_t m;
    if (tlv && tlv->get_tag(ctx, TLV_TAG, &m, 1) == 1 && m < OUT_COUNT) out_mode = m;
}

static void set_mode(uint8_t m) {
    if (m >= OUT_COUNT) return;
    out_mode = m;
    const btstack_tlv_t *tlv; void *ctx;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (tlv) tlv->store_tag(ctx, TLV_TAG, &m, 1);
    printf("USB keyboard: %s\n", out_name[m]);
}

static void on_tick(btstack_timer_source_t *ts) {
    static bool prev_restore, combo;
    static uint8_t prev_pa0;
    static absolute_time_t f11_until;

    if (usb_active) {
        if (matrix_c64_seen()) {
            printf("C64 detected - restarting in C64 mode\n");
            watchdog_reboot(0, 0, 50);
            for (;;) usb_kbd_task();
        }
        kb_state_t s;
        matrix_scan_get(&s);
        const kb_state_t *v = matrix_virtual();               // app / Bluetooth keyboards
        for (int i = 0; i < 8; i++) s.cols[i] |= v->cols[i];
        s.restore |= v->restore;

        // RESTORE + F key: choose the output mode, F keys are not sent then
        bool shift = (s.cols[KB_LSHIFT_COL] & (1u << KB_LSHIFT_ROW)) ||
                     (s.cols[KB_RSHIFT_COL] & (1u << KB_RSHIFT_ROW));
        uint8_t pa0 = s.cols[0];
        if (s.restore && !prev_restore) combo = false;
        if (s.restore) {
            uint8_t down = pa0 & (uint8_t)~prev_pa0;
            if (down & (1u << 4)) { set_mode(OUT_MISTER); combo = true; }        // F1
            if (down & (1u << 5)) { set_mode(OUT_PC_DE);  combo = true; }        // F3
            if (down & (1u << 6)) { set_mode(OUT_PC_US);  combo = true; }        // F5
            if (down & (1u << 3)) { set_mode(shift ? OUT_MAC_US : OUT_MAC_DE); combo = true; }   // F7 / F8
            s.cols[0] &= (uint8_t)~0x78;                                         // F1 F3 F5 F7
        }
        // MiSTer: RESTORE alone = F11 (sent when RESTORE is let go)
        if (!s.restore && prev_restore && !combo && out_mode == OUT_MISTER)
            f11_until = make_timeout_time_ms(F11_TAP_MS);
        prev_restore = s.restore;
        prev_pa0 = pa0;

        uint8_t mods = 0, keys[6] = {0};
        if (out_mode == OUT_MISTER) map_mister(&s, &mods, keys);
        else                        map_pc(&s, out_layout[out_mode], &mods, keys);
        if (!time_reached(f11_until)) {
            for (int i = 0; i < 6; i++) if (!keys[i]) { keys[i] = 0x44; break; }   // F11
        }
        usb_kbd_set(mods, keys);
    }
    usb_kbd_task();
    btstack_run_loop_set_timer(ts, TICK_MS);
    btstack_run_loop_add_timer(ts);
}

// ---------------------------------------------------------------------------

void usb_mode_start(bool usb_keyboard) {
    usb_active = usb_keyboard;
    if (usb_active) {
        load_mode();
        printf("USB keyboard mode (no C64): %s - RESTORE + F1/F3/F5/F7/F8 switches\n",
               out_name[out_mode]);
    }
    tick.process = &on_tick;
    btstack_run_loop_set_timer(&tick, TICK_MS);
    btstack_run_loop_add_timer(&tick);
}

bool usb_mode_active(void) { return usb_active; }

bool usb_mode_command(const char *cmd, size_t len) {
    // "!u" status, "!u0".."!u4" choose the output mode
    if (len < 2 || cmd[0] != '!' || tolower((unsigned char)cmd[1]) != 'u') return false;
    if (len == 3 && cmd[2] >= '0' && cmd[2] < '0' + OUT_COUNT) {
        set_mode((uint8_t)(cmd[2] - '0'));
    } else if (len == 2) {
        printf("mode: %s\n", usb_active ? "USB keyboard (no C64)" : "C64");
        printf("USB keyboard output: %s\n", out_name[out_mode]);
        for (int i = 0; i < OUT_COUNT; i++) printf("  !u%d  %s\n", i, out_name[i]);
    } else {
        return false;
    }
    return true;
}
