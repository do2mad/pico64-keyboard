// Pico64 Keyboard – USB device: serial console + USB keyboard
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
#ifndef PICO64_USB_KBD_H
#define PICO64_USB_KBD_H

#include <stdint.h>
#include <stdbool.h>

/** Start USB before stdio_init_all(). keyboard = false: serial console only (C64 mode). */
void usb_kbd_init(bool keyboard);
void usb_kbd_task(void);                                 // call often (every 1 ms)
void usb_kbd_set(uint8_t mods, const uint8_t keys[6]);   // new key state (sent when changed)

#endif
