// Pico64 Keyboard – C64 keyboard matrix emulation (core 1)
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
#ifndef PICO64_MATRIX_H
#define PICO64_MATRIX_H

#include <stdint.h>
#include <stdbool.h>

// Pin assignment (only GP0..GP22 are 5 V tolerant and free on the Pico 2 W;
// GP26..GP28 are ADC pins and NOT 5 V tolerant - never connect C64 lines there)
#define PIN_PA_BASE   0    // GP0..GP7   <- C64 CIA1 PA0..PA7 (column select, input only)
#define PIN_PB_BASE   8    // GP8..GP15  -> C64 CIA1 PB0..PB7 (rows, open drain: low or released)
#define PIN_RESTORE   16   // GP16       -> C64 RESTORE (open drain)

// Complete keyboard state: cols[n] = PB rows pressed in PA column n
typedef struct {
    uint8_t cols[8];
    bool    restore;
} kb_state_t;

// C64 matrix positions (PA column, PB row) of the modifier keys
#define KB_LSHIFT_COL 1
#define KB_LSHIFT_ROW 7
#define KB_CMDR_COL   7
#define KB_CMDR_ROW   5
#define KB_CTRL_COL   7
#define KB_CTRL_ROW   2

void matrix_init(void);                     // set up pins, start core 1
void matrix_apply(const kb_state_t *s);     // publish a new keyboard state
void matrix_release_all(void);

static inline void kb_press(kb_state_t *s, uint8_t col, uint8_t row) {
    s->cols[col & 7] |= (uint8_t)(1u << (row & 7));
}

#endif
