// Pico64 Keyboard – Bluetooth keyboards and gamepads (Bluepad32)
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// Bluepad32 (https://github.com/ricardoquesada/bluepad32, Apache-2.0) connects
// Bluetooth Classic and BLE keyboards and gamepads. This file is the Bluepad32
// "custom platform": it receives the parsed reports and turns them into
//
// * C64 key states (keymap.c) for keyboards,
// * joystick port 1 / 2 states (joystick.c) for gamepads,
// * a few C64 keys on gamepad buttons (SPACE, RETURN, F1, RUN/STOP).
//
// Gamepads:  D-pad or left stick = directions, A / B = fire,
//            X = SPACE, Y = RETURN, Select (-) = RUN/STOP, Start (+) = F1,
//            L1 + R1 together = swap joystick port.
//            The first gamepad plays on port 2 (most games), the second on port 1.
//
// Runs in the BTstack context (single threaded).

#include "input_bt.h"

#include <stdio.h>
#include <string.h>

#include <uni.h>

#include "sdkconfig.h"
#include "ble_service.h"
#include "joystick.h"
#include "keymap.h"
#include "keys.h"

#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico64 needs CONFIG_BLUEPAD32_PLATFORM_CUSTOM"
#endif

#define MAX_DEV        CONFIG_BLUEPAD32_MAX_DEVICES
#define STICK_DEADZONE 200          // axis range -512..511

typedef enum { DEV_NONE = 0, DEV_KEYBOARD, DEV_GAMEPAD, DEV_OTHER } dev_kind_t;

typedef struct {
    dev_kind_t kind;
    bool       connected;
    // keyboard
    uint8_t    order[UNI_KEYBOARD_PRESSED_KEYS_MAX];   // held keys, in the order they were pressed
    int        n_order;
    uint8_t    modifiers;
    // gamepad
    int        port;                // 1 or 2
    bool       swap_combo;          // L1+R1 held (edge detection)
    kb_state_t pad_keys;
} bt_dev_t;

static bt_dev_t devs[MAX_DEV];
static keymap_mode_t keymap_mode = KEYMAP_SYMBOLIC_DE;
static bool scanning;

// ---------------------------------------------------------------------------
// Keyboards

static void post_keyboards(void) {
    // All keyboards together form one source
    kb_state_t all = {0};
    for (int i = 0; i < MAX_DEV; i++) {
        if (devs[i].kind != DEV_KEYBOARD || !devs[i].connected) continue;
        kb_state_t s;
        keymap_build(keymap_mode, devs[i].modifiers, devs[i].order, devs[i].n_order, &s);
        for (int c = 0; c < 8; c++) all.cols[c] |= s.cols[c];
        all.restore |= s.restore;
    }
    keys_post(KEYSRC_BTKBD, &all);
}

static bool contains(const uint8_t *keys, int n, uint8_t k) {
    for (int i = 0; i < n; i++) if (keys[i] == k) return true;
    return false;
}

static void on_keyboard(bt_dev_t *dv, const uni_keyboard_t *kb) {
    // Current key set (without error / empty codes)
    uint8_t now[UNI_KEYBOARD_PRESSED_KEYS_MAX];
    int n_now = 0;
    for (int i = 0; i < UNI_KEYBOARD_PRESSED_KEYS_MAX; i++) {
        uint8_t k = kb->pressed_keys[i];
        if (k >= 0x04 && !contains(now, n_now, k)) now[n_now++] = k;
    }

    // Keep the press order: drop released keys, append new ones
    uint8_t order[UNI_KEYBOARD_PRESSED_KEYS_MAX];
    int n = 0;
    bool f10_new = false;
    for (int i = 0; i < dv->n_order; i++)
        if (contains(now, n_now, dv->order[i])) order[n++] = dv->order[i];
    for (int i = 0; i < n_now; i++)
        if (!contains(order, n, now[i])) {
            order[n++] = now[i];
            if (now[i] == HID_KEY_F10) f10_new = true;
        }

    bool changed = (n != dv->n_order) || (kb->modifiers != dv->modifiers) ||
                   memcmp(order, dv->order, (size_t)n) != 0;
    memcpy(dv->order, order, (size_t)n);
    dv->n_order = n;
    dv->modifiers = kb->modifiers;

    if (f10_new) input_bt_toggle_keymap();
    else if (changed) post_keyboards();
}

// ---------------------------------------------------------------------------
// Gamepads

static void post_pad_keys(void) {
    kb_state_t all = {0};
    for (int i = 0; i < MAX_DEV; i++) {
        if (devs[i].kind != DEV_GAMEPAD || !devs[i].connected) continue;
        for (int c = 0; c < 8; c++) all.cols[c] |= devs[i].pad_keys.cols[c];
    }
    keys_post(KEYSRC_PAD, &all);
}

static int free_port(int self) {
    bool used[3] = { false, false, false };
    for (int i = 0; i < MAX_DEV; i++)
        if (i != self && devs[i].kind == DEV_GAMEPAD && devs[i].connected) used[devs[i].port] = true;
    return used[2] && !used[1] ? 1 : 2;
}

static void on_gamepad(int idx, bt_dev_t *dv, const uni_gamepad_t *gp) {
    if (dv->port != 1 && dv->port != 2) dv->port = free_port(idx);

    // L1 + R1: swap port
    bool combo = (gp->buttons & BUTTON_SHOULDER_L) && (gp->buttons & BUTTON_SHOULDER_R);
    if (combo && !dv->swap_combo) {
        joystick_set(idx, dv->port, 0);
        dv->port = 3 - dv->port;
        printf("gamepad %d now on joystick port %d\n", idx, dv->port);
    }
    dv->swap_combo = combo;

    uint8_t joy = 0;
    if (!combo) {
        if ((gp->dpad & DPAD_UP)    || gp->axis_y < -STICK_DEADZONE) joy |= JOY_UP;
        if ((gp->dpad & DPAD_DOWN)  || gp->axis_y >  STICK_DEADZONE) joy |= JOY_DOWN;
        if ((gp->dpad & DPAD_LEFT)  || gp->axis_x < -STICK_DEADZONE) joy |= JOY_LEFT;
        if ((gp->dpad & DPAD_RIGHT) || gp->axis_x >  STICK_DEADZONE) joy |= JOY_RIGHT;
        if (gp->buttons & (BUTTON_A | BUTTON_B)) joy |= JOY_FIRE;
    }
    joystick_set(idx, dv->port, joy);

    kb_state_t k = {0};
    if (gp->buttons & BUTTON_X)               kb_press_code(&k, 0x3C);   // SPACE
    if (gp->buttons & BUTTON_Y)               kb_press_code(&k, 0x01);   // RETURN
    if (gp->misc_buttons & MISC_BUTTON_SELECT) kb_press_code(&k, 0x3F);  // RUN/STOP
    if (gp->misc_buttons & MISC_BUTTON_START)  kb_press_code(&k, 0x04);  // F1
    if (memcmp(&k, &dv->pad_keys, sizeof(k)) != 0) {
        dv->pad_keys = k;
        post_pad_keys();
    }
}

// ---------------------------------------------------------------------------
// Bluepad32 platform callbacks

static void pf_init(int argc, const char **argv) {
    (void)argc; (void)argv;
    memset(devs, 0, sizeof(devs));
}

static void pf_on_init_complete(void) {
    // Bluetooth is up: start the app service and look for keyboards / gamepads.
    // Known (paired) devices reconnect, new ones in pairing mode are paired.
    ble_service_start();
    uni_bt_start_scanning_and_autoconnect_unsafe();
    scanning = true;
    printf("Bluetooth keyboards / gamepads: scanning (keyboard mapping: %s)\n", keymap_mode_name(keymap_mode));
    uni_bt_list_keys_unsafe();
}

static uni_error_t pf_on_device_discovered(bd_addr_t addr, const char *name, uint16_t cod, uint8_t rssi) {
    (void)addr; (void)cod; (void)rssi;
    printf("found %s\n", (name && name[0]) ? name : "(no name)");
    return UNI_ERROR_SUCCESS;          // keyboards, gamepads, joysticks are all welcome
}

static void pf_on_device_connected(uni_hid_device_t *d) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= MAX_DEV) return;
    memset(&devs[idx], 0, sizeof(devs[idx]));
}

static void pf_on_device_disconnected(uni_hid_device_t *d) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= MAX_DEV) return;
    bt_dev_t *dv = &devs[idx];
    printf("disconnected: %s\n", d->name);
    dev_kind_t kind = dv->kind;
    memset(dv, 0, sizeof(*dv));
    if (kind == DEV_KEYBOARD) post_keyboards();
    if (kind == DEV_GAMEPAD) {
        joystick_set(idx, 0, 0);
        post_pad_keys();
    }
}

static uni_error_t pf_on_device_ready(uni_hid_device_t *d) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= MAX_DEV) return UNI_ERROR_INVALID_DEVICE;
    bt_dev_t *dv = &devs[idx];
    dv->connected = true;
    if (uni_hid_device_is_keyboard(d)) {
        dv->kind = DEV_KEYBOARD;
        printf("keyboard connected: %s (mapping: %s, F10 switches)\n", d->name, keymap_mode_name(keymap_mode));
    } else if (uni_hid_device_is_gamepad(d)) {
        dv->kind = DEV_GAMEPAD;
        dv->port = free_port(idx);
        printf("gamepad connected: %s -> joystick port %d (L1+R1 swaps)\n", d->name, dv->port);
    } else {
        dv->kind = DEV_OTHER;
        printf("connected (not used): %s\n", d->name);
    }
    if (d->report_parser.set_player_leds != NULL && dv->kind == DEV_GAMEPAD)
        d->report_parser.set_player_leds(d, (uint8_t)dv->port);
    return UNI_ERROR_SUCCESS;
}

static void pf_on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= MAX_DEV) return;
    bt_dev_t *dv = &devs[idx];
    switch (ctl->klass) {
    case UNI_CONTROLLER_CLASS_KEYBOARD:
        dv->kind = DEV_KEYBOARD;
        dv->connected = true;
        on_keyboard(dv, &ctl->keyboard);
        break;
    case UNI_CONTROLLER_CLASS_GAMEPAD:
        if (dv->kind != DEV_GAMEPAD) { dv->kind = DEV_GAMEPAD; dv->port = 0; }
        dv->connected = true;
        on_gamepad(idx, dv, &ctl->gamepad);
        break;
    default:
        break;                         // mice, balance boards ... not used
    }
}

static const uni_property_t *pf_get_property(uni_property_idx_t idx) {
    (void)idx;
    return NULL;
}

static void pf_on_oob_event(uni_platform_oob_event_t event, void *data) {
    (void)event; (void)data;
}

struct uni_platform *input_bt_platform(void) {
    static struct uni_platform plat = {
        .name = "Pico64",
        .init = pf_init,
        .on_init_complete = pf_on_init_complete,
        .on_device_discovered = pf_on_device_discovered,
        .on_device_connected = pf_on_device_connected,
        .on_device_disconnected = pf_on_device_disconnected,
        .on_device_ready = pf_on_device_ready,
        .on_oob_event = pf_on_oob_event,
        .on_controller_data = pf_on_controller_data,
        .get_property = pf_get_property,
    };
    return &plat;
}

// ---------------------------------------------------------------------------
// Console commands / status

int input_bt_device_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_DEV; i++) if (devs[i].connected) n++;
    return n;
}

void input_bt_toggle_keymap(void) {
    keymap_mode = (keymap_mode == KEYMAP_SYMBOLIC_DE) ? KEYMAP_POSITIONAL : KEYMAP_SYMBOLIC_DE;
    printf("keyboard mapping: %s\n", keymap_mode_name(keymap_mode));
    post_keyboards();
}

void input_bt_status(void) {
    printf("app: %s\n", ble_service_connected() ? "connected" : "not connected");
    printf("keyboard mapping: %s\n", keymap_mode_name(keymap_mode));
    printf("scanning for new devices: %s\n", scanning ? "on" : "off");
    for (int i = 0; i < MAX_DEV; i++) {
        if (!devs[i].connected) continue;
        uni_hid_device_t *d = uni_hid_device_get_instance_for_idx(i);
        const char *name = d ? d->name : "?";
        if (devs[i].kind == DEV_GAMEPAD) printf("  #%d gamepad  %s, joystick port %d\n", i, name, devs[i].port);
        else if (devs[i].kind == DEV_KEYBOARD) printf("  #%d keyboard %s\n", i, name);
        else printf("  #%d other    %s\n", i, name);
    }
}

void input_bt_forget_all(void) {
    printf("forgetting all paired keyboards / gamepads\n");
    uni_bt_del_keys_unsafe();
}

void input_bt_toggle_scan(void) {
    if (scanning) uni_bt_stop_scanning_unsafe();
    else          uni_bt_start_scanning_and_autoconnect_unsafe();
    scanning = !scanning;
    printf("scanning for new devices: %s\n", scanning ? "on" : "off");
}
