// Pico64 Keyboard – C64 keyboard matrix and joystick emulation (core 1)
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// The C64 KERNAL (and most programs) scan the keyboard by pulling one or more
// CIA1 port A lines (columns) low and reading port B (rows). Core 1 watches
// PA0..7 in a tight loop and, via a 256-entry lookup table, pulls exactly those
// PB lines low whose keys are "pressed" in any of the active columns - just like
// the real key switches would. Reaction time is well below 100 ns, the C64
// reads port B at the earliest a few microseconds after writing port A.
//
// Joysticks share these lines: port 1 = PB0..PB4, port 2 = PA0..PA4. A real
// joystick simply shorts the line to GND. Joystick 1 bits are ORed into every
// table entry (they pull PB low regardless of the column), joystick 2 pulls
// PA0..PA4 low directly - core 1 then sees these columns as active, exactly
// like the C64 does with a real joystick in port 2.
//
// All C64 lines are only ever driven LOW or released (output enable on/off with
// output value 0). They are never driven HIGH, so the real keyboard, joysticks
// and the CIA can stay connected in parallel.
//
// USB keyboard mode (no C64): core 1 scans the real C64 keyboard itself - one
// PA column after the other is pulled low, the PB rows are read with the
// Pico's pull-ups. The result is turned into USB key presses (usb_mode.c).
// Between two scans all lines are released; if a PA line is low then, a
// running C64 is scanning too (usb_mode.c then restarts in C64 mode).

#include "matrix.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "hardware/timer.h"

// Two lookup tables (row mask for every PA value); core 0 fills the inactive
// one and then switches the pointer. Placed in RAM, 256-byte aligned.
static uint8_t tables[2][256] __attribute__((aligned(256)));
static uint8_t *volatile active_table = tables[0];

static kb_state_t current_kb;   // last keyboard state (core 0 only)
static uint8_t joy1;            // joystick port 1 bits, part of every table entry
static bool    usb_mode;        // true: no C64, core 1 scans the real keyboard

// USB mode: debounced state of the real keyboard, written by core 1
static volatile uint32_t scan_seq;          // odd while core 1 writes
static kb_state_t        scan_state;
static volatile bool     c64_seen;          // a running C64 pulls PA lines low

static void __not_in_flash_func(core1_main)(void) {
    // Allow core 0 to pause this core while it writes the flash (Bluetooth
    // pairing keys). The pause handler runs from RAM; the pins keep their state.
    flash_safe_execute_core_init();

    uint32_t current = 0;
    for (;;) {
        const uint8_t *t = active_table;
        uint32_t pa = (sio_hw->gpio_in >> PIN_PA_BASE) & 0xFFu;
        uint32_t want = t[pa];
        if (want != current) {
            sio_hw->gpio_oe_set = (want & ~current) << PIN_PB_BASE;   // pull low
            sio_hw->gpio_oe_clr = (current & ~want) << PIN_PB_BASE;   // release
            current = want;
        }
    }
}

static void init_open_drain(uint pin, bool hysteresis) {
    gpio_init(pin);              // SIO function, input, clears pad isolation
    gpio_disable_pulls(pin);     // the CIA has its own pull-ups
    gpio_put(pin, 0);            // output value stays 0 forever
    gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);   // low level as close to 0 V as possible
    gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
    gpio_set_input_hysteresis_enabled(pin, hysteresis);
    gpio_set_dir(pin, GPIO_IN);  // released
}

static void build_table(void) {
    uint8_t *t = (active_table == tables[0]) ? tables[1] : tables[0];
    for (uint32_t pa = 0; pa < 256; pa++) {
        uint8_t rows = joy1;
        for (uint32_t col = 0; col < 8; col++)
            if (!(pa & (1u << col))) rows |= current_kb.cols[col];   // column active = low
        t[pa] = rows;
    }
    __dmb();
    active_table = t;
}

void matrix_init(void) {
    for (uint i = 0; i < 8; i++) {
        init_open_drain(PIN_PA_BASE + i, true);    // read by core 1, pulled low for joystick 2
        init_open_drain(PIN_PB_BASE + i, false);
    }
    init_open_drain(PIN_RESTORE, false);

    for (int i = 0; i < 256; i++) tables[0][i] = tables[1][i] = 0;
    active_table = tables[0];
}

void matrix_start_c64(void) {
    usb_mode = false;
    multicore_launch_core1(core1_main);
}

// ---------------------------------------------------------------------------
// USB keyboard mode

#define PA_MASK (0xFFu << PIN_PA_BASE)
#define PB_MASK (0xFFu << PIN_PB_BASE)

static void set_pullups(bool on) {
    for (uint i = 0; i < 8; i++) {
        gpio_set_pulls(PIN_PA_BASE + i, on, false);
        gpio_set_pulls(PIN_PB_BASE + i, on, false);
    }
    gpio_set_pulls(PIN_RESTORE, on, false);
}

bool matrix_detect_c64(uint32_t ms, void (*idle)(void)) {
    // A running C64 scans the keyboard at least every 1/60 s by pulling PA lines
    // low. Without a C64 the lines stay high (Pico pull-ups; the CIA pull-ups
    // pull to 5 V, the inputs are 5 V tolerant).
    set_pullups(true);
    busy_wait_us(200);
    bool seen = false;
    absolute_time_t end = make_timeout_time_ms(ms);
    while (!time_reached(end) && !seen) {
        if (((sio_hw->gpio_in & PA_MASK) >> PIN_PA_BASE) != 0xFF) seen = true;
        if (idle) idle();
    }
    if (seen) set_pullups(false);   // back to C64 mode: the CIA has its own pull-ups
    return seen;
}

static void core1_scan(void) {
    flash_safe_execute_core_init();          // see core1_main
    kb_state_t last = {0};
    int stable = 0, c64_low = 0;
    for (;;) {
        // all lines released: a low PA line now means a C64 is scanning too
        if (((sio_hw->gpio_in & PA_MASK) >> PIN_PA_BASE) != 0xFF) {
            if (++c64_low >= 5) c64_seen = true;
        } else {
            c64_low = 0;
        }

        kb_state_t s = {0};
        for (uint col = 0; col < 8; col++) {
            sio_hw->gpio_oe_set = 1u << (PIN_PA_BASE + col);       // column low
            busy_wait_us(10);
            s.cols[col] = (uint8_t)~((sio_hw->gpio_in & PB_MASK) >> PIN_PB_BASE);
            sio_hw->gpio_oe_clr = 1u << (PIN_PA_BASE + col);       // release
            busy_wait_us(40);                                      // pull-up needs time
        }
        s.restore = !gpio_get(PIN_RESTORE);

        // debounce: publish a state once it was read 3 times in a row (~3 ms)
        if (memcmp(&s, &last, sizeof(s)) == 0) {
            if (stable < 3 && ++stable == 3) {
                scan_seq++;
                __dmb();
                scan_state = s;
                __dmb();
                scan_seq++;
            }
        } else {
            last = s;
            stable = 0;
        }
        busy_wait_us(500);
    }
}

void matrix_start_usb(void) {
    usb_mode = true;
    set_pullups(true);
    multicore_launch_core1(core1_scan);
}

bool matrix_usb_mode(void) { return usb_mode; }
bool matrix_c64_seen(void) { return c64_seen; }

void matrix_scan_get(kb_state_t *out) {
    uint32_t a, b;
    do {
        a = scan_seq;
        __dmb();
        *out = scan_state;
        __dmb();
        b = scan_seq;
    } while (a != b || (a & 1));
}

const kb_state_t *matrix_virtual(void) { return &current_kb; }

void matrix_apply(const kb_state_t *s) {
    current_kb = *s;
    if (usb_mode) return;        // app / Bluetooth keys go to USB (usb_mode.c)
    build_table();
    gpio_set_dir(PIN_RESTORE, s->restore ? GPIO_OUT : GPIO_IN);
}

void matrix_release_all(void) {
    kb_state_t none = {0};
    matrix_apply(&none);
    matrix_set_joystick(0, 0);
}

void matrix_set_joystick(uint8_t port1, uint8_t port2) {
    if (usb_mode) return;        // no joystick ports without a C64
    port1 &= 0x1F;
    port2 &= 0x1F;
    if (port1 != joy1) {
        joy1 = port1;
        build_table();
    }
    // Port 2: PA0..PA4 - only touched here (core 1 only changes PB bits).
    // The SIO set/clear registers are atomic, so both cores can use them.
    sio_hw->gpio_oe_set = (uint32_t)port2 << PIN_PA_BASE;
    sio_hw->gpio_oe_clr = (uint32_t)(~port2 & 0x1F) << PIN_PA_BASE;
}
