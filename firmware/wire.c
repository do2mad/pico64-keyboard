// Pico64 Keyboard – cable link from the CBM2 Keyboard adapter (CBM-II keyboard)
// Copyright (c) 2026 Martin Oswald (1mhz.de) - SPDX-License-Identifier: MIT
//
// Instead of Bluetooth the CBM2 Keyboard can be connected with a cable:
//   CBM2 GP28 (UART0 TX) -- 330 R --> Pico64 GP17 (pin 22, UART0 RX)
//   detect line, 5 V (Pico64 VSYS, pin 39) and GND - see the CBM2 Keyboard README.
// 115200 baud 8N1, frame: 0xC6 0x4B flags col0..col7 check
//   flags bit 3 = RESTORE, colN = PB rows pressed in PA column N (like the matrix
//   characteristic of the app), check = XOR of flags..col7 XOR 0x5A.
// The sender repeats the state every 100 ms; without a valid frame for 300 ms the
// keys of this source are released. The keys go straight to the matrix (like a real
// keyboard, no minimum hold times - fast for games), see keys_post(). GP17 has a pull-up, so nothing happens
// without a cable.

#include "wire.h"

#include <stdio.h>
#include <string.h>
#include "btstack.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "keys.h"

#define WIRE_UART   uart0
#define PIN_RX      17
#define BAUD        115200
#define POLL_MS     1
#define TIMEOUT_MS  300

static btstack_timer_source_t timer;
static uint8_t frame[12];
static int pos;
static bool active;
static absolute_time_t last_frame;
static kb_state_t last;

static void got_frame(void) {
    uint8_t x = 0x5A;
    for (int i = 2; i < 11; i++) x ^= frame[i];
    if (x != frame[11]) return;                         // damaged
    kb_state_t s = {0};
    memcpy(s.cols, &frame[3], 8);
    s.restore = (frame[2] & 0x08) != 0;
    last_frame = get_absolute_time();
    if (!active) { active = true; printf("cable: CBM2 keyboard connected\n"); }
    if (memcmp(&s, &last, sizeof(s)) != 0) {            // keep-alive frames are not posted
        last = s;
        keys_post(KEYSRC_WIRE, &s);
    }
}

static void poll(btstack_timer_source_t *ts) {
    while (uart_is_readable(WIRE_UART)) {
        uint8_t c = (uint8_t)uart_getc(WIRE_UART);
        if (pos == 0 && c != 0xC6) continue;
        if (pos == 1 && c != 0x4B) { pos = (c == 0xC6) ? 1 : 0; continue; }
        frame[pos++] = c;
        if (pos == sizeof(frame)) { got_frame(); pos = 0; }
    }
    if (active && absolute_time_diff_us(last_frame, get_absolute_time()) > TIMEOUT_MS * 1000) {
        active = false;
        memset(&last, 0, sizeof(last));
        keys_release_source(KEYSRC_WIRE);
        printf("cable: CBM2 keyboard gone - keys released\n");
    }
    btstack_run_loop_set_timer(ts, POLL_MS);
    btstack_run_loop_add_timer(ts);
}

void wire_init(void) {
    uart_init(WIRE_UART, BAUD);
    gpio_set_function(PIN_RX, GPIO_FUNC_UART);
    gpio_pull_up(PIN_RX);
    timer.process = &poll;
    btstack_run_loop_set_timer(&timer, POLL_MS);
    btstack_run_loop_add_timer(&timer);
}

bool wire_active(void) { return active; }
