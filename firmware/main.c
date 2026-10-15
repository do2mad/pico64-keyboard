// Pico64 Keyboard – C64 keyboard matrix adapter with Raspberry Pi Pico 2 W
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// Core 1: emulates the C64 keyboard matrix and the joystick lines (matrix.c)
// Core 0: Bluetooth (BTstack + Bluepad32): app service, Bluetooth keyboards and
//         gamepads, key scheduling, text feed, USB console
//
// USB console (any terminal, 115200 baud): every line you type is typed on the
// C64 followed by RETURN. Letters are sent unshifted (= upper case on the C64),
// tokens like ~clr~ ~home~ ~f1~ work as in the app's text window.
// Commands (a line with only this):
//   d    key log on/off           !s   status
//   !p   keyboard mapping (symbolic DE / positional, same as F10)
//   !b   scanning for new Bluetooth devices on/off
//   !x   forget all paired Bluetooth keyboards / gamepads
//   !u   USB keyboard mode status, !u0..!u4 choose its output (see usb_mode.c)
//
// USB keyboard mode: powered by USB and no C64 scanning the keyboard -> the
// real C64 keyboard on the board becomes a USB keyboard (PC, Mac, MiSTer).
// Without USB power the Pico can only be powered by the C64 -> C64 mode.

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include <uni.h>

#include "matrix.h"
#include "keys.h"
#include "joystick.h"
#include "input_bt.h"
#include "textfeed.h"
#include "usb_kbd.h"
#include "usb_mode.h"

#define PICO64_VERSION  "0.12.0"
#define CONSOLE_POLL_MS 20

static btstack_timer_source_t console_timer;
static char line[TEXTFEED_MAX - 8];
static size_t line_len;

static bool is_cmd(const char *cmd) {
    size_t n = strlen(cmd);
    if (line_len != n) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)line[i]) != cmd[i]) return false;
    return true;
}

static bool handle_command(void) {
    if (is_cmd("d"))  { keys_toggle_debug();        return true; }
    if (is_cmd("!p")) { input_bt_toggle_keymap();   return true; }
    if (is_cmd("!s")) { input_bt_status();          return true; }
    if (is_cmd("!b")) { input_bt_toggle_scan();     return true; }
    if (is_cmd("!x")) { input_bt_forget_all();      return true; }
    if (usb_mode_command(line, line_len))           return true;
    return false;
}

static void console_poll(btstack_timer_source_t *ts) {
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (line_len == 0 && c == '\n') continue;       // CR LF
            if (handle_command()) {
                line_len = 0;
                continue;
            }
            static char out[TEXTFEED_MAX];
            size_t n = 0;
            for (size_t i = 0; i < line_len && n < sizeof(out) - 6; i++)
                out[n++] = (char)tolower((unsigned char)line[i]);
            memcpy(&out[n], "~ret~", 5);
            n += 5;
            if (keys_type_text(out, n)) printf("typing: %.*s\n", (int)line_len, line);
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
    matrix_init();               // all C64 lines released first

    if (cyw43_arch_init()) return -1;

    // C64 or USB keyboard? No USB power -> only the C64 can power us. With USB
    // power, a running C64 still shows itself by scanning the keyboard (a C64
    // that is switched off but connected pulls the lines low - also C64 mode).
    bool vbus = cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN);
    bool usb_keyboard = vbus && !matrix_detect_c64(600, NULL);

    // USB: serial console (+ keyboard in USB keyboard mode); TinyUSB is run by
    // usb_mode.c every 1 ms
    usb_kbd_init(usb_keyboard);
    stdio_init_all();

    if (usb_keyboard) matrix_start_usb();
    else              matrix_start_c64();
    printf("Pico64 Keyboard v" PICO64_VERSION " starting (%s)\n",
           usb_keyboard ? "USB keyboard, no C64" : "C64");
    usb_mode_start(usb_keyboard);

    keys_init();
    joystick_init();

    console_timer.process = &console_poll;
    btstack_run_loop_set_timer(&console_timer, CONSOLE_POLL_MS);
    btstack_run_loop_add_timer(&console_timer);

    // Bluepad32 sets up BTstack (L2CAP, Security Manager, Classic + LE) and
    // switches Bluetooth on. When it is ready, input_bt.c starts the app
    // service (GATT + advertising) and the search for keyboards / gamepads.
    uni_platform_set_custom(input_bt_platform());
    uni_init(0, NULL);

    btstack_run_loop_execute();  // does not return
    return 0;
}
