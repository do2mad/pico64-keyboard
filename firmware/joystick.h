// Pico64 Keyboard – joystick ports 1 and 2 from Bluetooth gamepads
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
#ifndef PICO64_JOYSTICK_H
#define PICO64_JOYSTICK_H

#include <stdint.h>

#define JOY_SOURCES 4      // one per Bluetooth gamepad

void joystick_init(void);
/** Set the JOY_* bits of one source (gamepad) on port 1 or 2. port 0 = none. */
void joystick_set(int source, int port, uint8_t bits);
void joystick_release_all(void);

#endif
