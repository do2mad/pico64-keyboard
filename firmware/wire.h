// Pico64 Keyboard – cable link from the CBM2 Keyboard adapter (CBM-II keyboard)
// Copyright (c) 2026 Martin Oswald (1mhz.de) - SPDX-License-Identifier: MIT
#ifndef PICO64_WIRE_H
#define PICO64_WIRE_H

#include <stdbool.h>

void wire_init(void);        // start receiving (BTstack context)
bool wire_active(void);      // frames are arriving

#endif
