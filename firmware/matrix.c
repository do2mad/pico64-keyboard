// Pico64 Keyboard – C64 keyboard matrix emulation (core 1)
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// The C64 KERNAL (and most programs) scan the keyboard by pulling one or more
// CIA1 port A lines (columns) low and reading port B (rows). Core 1 watches
// PA0..7 in a tight loop and, via a 256-entry lookup table, pulls exactly those
// PB lines low whose keys are "pressed" in any of the active columns - just like
// the real key switches would. Reaction time is well below 100 ns, the C64
// reads port B at the earliest a few microseconds after writing port A.
//
// The PB and RESTORE pins are only ever driven LOW or released (output enable
// on/off with output value 0). They never drive HIGH, so the real keyboard,
// joysticks and the CIA can stay connected in parallel.

#include "matrix.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"

#define PA_MASK (0xFFu << PIN_PA_BASE)
#define PB_MASK (0xFFu << PIN_PB_BASE)

// Two lookup tables (row mask for every PA value); core 0 fills the inactive
// one and then switches the pointer. Placed in RAM, 256-byte aligned.
static uint8_t tables[2][256] __attribute__((aligned(256)));
static uint8_t *volatile active_table = tables[0];

static void __not_in_flash_func(core1_main)(void) {
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

static void init_open_drain(uint pin) {
    gpio_init(pin);              // SIO function, input, clears pad isolation
    gpio_disable_pulls(pin);
    gpio_put(pin, 0);            // output value stays 0 forever
    gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);   // low level as close to 0 V as possible
    gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
    gpio_set_dir(pin, GPIO_IN);  // released
}

void matrix_init(void) {
    for (uint i = 0; i < 8; i++) {
        uint pa = PIN_PA_BASE + i;
        gpio_init(pa);
        gpio_set_dir(pa, GPIO_IN);
        gpio_disable_pulls(pa);                  // the CIA has its own pull-ups
        gpio_set_input_hysteresis_enabled(pa, true);
        init_open_drain(PIN_PB_BASE + i);
    }
    init_open_drain(PIN_RESTORE);

    for (int i = 0; i < 256; i++) tables[0][i] = tables[1][i] = 0;
    active_table = tables[0];

    multicore_launch_core1(core1_main);
}

void matrix_apply(const kb_state_t *s) {
    uint8_t *t = (active_table == tables[0]) ? tables[1] : tables[0];
    for (uint32_t pa = 0; pa < 256; pa++) {
        uint8_t rows = 0;
        for (uint32_t col = 0; col < 8; col++)
            if (!(pa & (1u << col))) rows |= s->cols[col];   // column active = low
        t[pa] = rows;
    }
    __dmb();
    active_table = t;
    gpio_set_dir(PIN_RESTORE, s->restore ? GPIO_OUT : GPIO_IN);
}

void matrix_release_all(void) {
    kb_state_t none = {0};
    matrix_apply(&none);
}
