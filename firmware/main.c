// Pico64 Keyboard – C64 keyboard matrix adapter with Raspberry Pi Pico 2 W
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// Core 1: emulates the C64 keyboard matrix (matrix.c)
// Core 0: Bluetooth LE (BTstack), key scheduling, text feed, USB console
//
// USB console (any terminal, 115200 baud): every line you type is typed on the
// C64 followed by RETURN. Letters are sent unshifted (= upper case on the C64),
// tokens like ~clr~ ~home~ ~f1~ work as in the app's text window.

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

#include "matrix.h"
#include "ble_service.h"
#include "textfeed.h"

#define PICO64_VERSION  "0.9.0"
#define CONSOLE_POLL_MS 20

static btstack_timer_source_t console_timer;
static char line[TEXTFEED_MAX - 8];
static size_t line_len;

static void console_poll(btstack_timer_source_t *ts) {
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (line_len == 0 && c == '\n') continue;       // CR LF
            if (line_len == 1 && (line[0] == 'd' || line[0] == 'D')) {   // "d" = key log on/off
                ble_service_toggle_debug();
                line_len = 0;
                continue;
            }
            static char out[TEXTFEED_MAX];
            size_t n = 0;
            for (size_t i = 0; i < line_len && n < sizeof(out) - 6; i++)
                out[n++] = (char)tolower((unsigned char)line[i]);
            memcpy(&out[n], "~ret~", 5);
            n += 5;
            if (ble_service_type_text(out, n)) printf("typing: %.*s\n", (int)line_len, line);
            else                               printf("busy - try again\n");
            line_len = 0;
        } else if ((c == 8 || c == 127) && line_len > 0) {
            line_len--;
        } else if (c >= 32 && c < 127 && line_len < sizeof(line)) {
            line[line_len++] = (char)c;
        }
    }
    btstack_run_loop_set_timer(ts, CONSOLE_POLL_MS);
    btstack_run_loop_add_timer(ts);
}

int main(void) {
    stdio_init_all();

    matrix_init();               // pins released first, then core 1 starts

    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return -1;
    }

    ble_service_init();

    console_timer.process = &console_poll;
    btstack_run_loop_set_timer(&console_timer, CONSOLE_POLL_MS);
    btstack_run_loop_add_timer(&console_timer);

    hci_power_control(HCI_POWER_ON);
    printf("Pico64 Keyboard v" PICO64_VERSION " starting\n");

    for (;;) {
        async_context_poll(cyw43_arch_async_context());
        async_context_wait_for_work_until(cyw43_arch_async_context(), at_the_end_of_time);
    }
}
