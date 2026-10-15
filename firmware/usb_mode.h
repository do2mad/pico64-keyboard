// Pico64 Keyboard – USB keyboard mode: the real C64 keyboard as USB keyboard
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
#ifndef PICO64_USB_MODE_H
#define PICO64_USB_MODE_H

#include <stdbool.h>
#include <stddef.h>

/** Start the 1 ms USB timer (BTstack context). usb_keyboard = no C64: scan the
 *  real keyboard and send it via USB. In C64 mode only the serial console runs. */
void usb_mode_start(bool usb_keyboard);
bool usb_mode_active(void);
/** Console commands "!u" (status) and "!u0".."!u4" (output mode). */
bool usb_mode_command(const char *cmd, size_t len);

#endif
