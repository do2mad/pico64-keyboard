// Pico64 Keyboard – joystick ports 1 and 2 from Bluetooth gamepads
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// Joystick states bypass the key queue - games read the joystick every frame
// and want it immediately. Only one thing is added: every direction or fire
// press stays active for at least JOY_MIN_HOLD_MS, so a very short tap on the
// gamepad is never lost between two frames (PAL: 20 ms).
//
// Runs in the BTstack context (single threaded).

#include "joystick.h"

#include "btstack.h"
#include "pico/time.h"

#include "matrix.h"

#define JOY_MIN_HOLD_MS 40

typedef struct {
    int     port;          // 1, 2 or 0 (not assigned)
    uint8_t bits;          // requested JOY_* bits
} joy_source_t;

static joy_source_t sources[JOY_SOURCES];
static uint32_t pressed_at[2][5];      // per port and bit: time of the last press
static uint8_t  shown[2];              // bits currently applied per port
static btstack_timer_source_t hold_timer;
static bool timer_running;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

static void update(void);

static void hold_tick(btstack_timer_source_t *ts) {
    (void)ts;
    timer_running = false;
    update();
}

static void update(void) {
    uint8_t want[2] = {0, 0};
    for (int i = 0; i < JOY_SOURCES; i++)
        if (sources[i].port == 1 || sources[i].port == 2)
            want[sources[i].port - 1] |= sources[i].bits;

    uint32_t t = now_ms();
    bool pending = false;
    for (int p = 0; p < 2; p++) {
        uint8_t out = 0;
        for (int b = 0; b < 5; b++) {
            uint8_t m = (uint8_t)(1u << b);
            if (want[p] & m) {
                if (!(shown[p] & m)) pressed_at[p][b] = t;   // new press
                out |= m;
            } else if ((shown[p] & m) && (t - pressed_at[p][b]) < JOY_MIN_HOLD_MS) {
                out |= m;                                   // keep it a little longer
                pending = true;
            }
        }
        shown[p] = out;
    }
    matrix_set_joystick(shown[0], shown[1]);

    if (pending && !timer_running) {
        timer_running = true;
        btstack_run_loop_set_timer(&hold_timer, 10);
        btstack_run_loop_add_timer(&hold_timer);
    }
}

void joystick_init(void) {
    hold_timer.process = &hold_tick;
}

void joystick_set(int source, int port, uint8_t bits) {
    if (source < 0 || source >= JOY_SOURCES) return;
    sources[source].port = port;
    sources[source].bits = bits & 0x1F;
    update();
}

void joystick_release_all(void) {
    for (int i = 0; i < JOY_SOURCES; i++) sources[i].bits = 0;
    shown[0] = shown[1] = 0;
    matrix_set_joystick(0, 0);
}
