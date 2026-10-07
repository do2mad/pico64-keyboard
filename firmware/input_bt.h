// Pico64 Keyboard – Bluetooth keyboards and gamepads (Bluepad32)
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
#ifndef PICO64_INPUT_BT_H
#define PICO64_INPUT_BT_H

struct uni_platform;

/** Bluepad32 platform; pass to uni_platform_set_custom() before uni_init(). */
struct uni_platform *input_bt_platform(void);

int  input_bt_device_count(void);   // connected keyboards + gamepads

// USB console commands (run in the BTstack context)
void input_bt_toggle_keymap(void);
void input_bt_status(void);
void input_bt_forget_all(void);
void input_bt_toggle_scan(void);

#endif
