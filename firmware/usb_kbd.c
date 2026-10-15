// Pico64 Keyboard – USB device: serial console (CDC, used by stdio) + boot keyboard (HID)
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
// Based on the TinyUSB cdc / hid examples (MIT, Ha Thach).
//
// C64 mode: only the serial console, with the IDs of the Pico SDK (as before).
// USB keyboard mode (no C64): console + keyboard, fed by usb_mode.c.
// The mode is fixed before USB starts, so the computer never sees a keyboard
// that does nothing (macOS would ask to identify it).
// tusb.h is only included here (it clashes with btstack.h).

#include "usb_kbd.h"

#include <string.h>
#include "tusb.h"
#include "pico/unique_id.h"

// USB keyboard mode: pid.codes test VID/PID (https://pid.codes/1209/0001/)
#define USB_VID 0x1209
#define USB_PID 0x0001
// C64 mode: Raspberry Pi Pico SDK stdio IDs (as up to v0.11)
#define SDK_VID 0x2E8A
#define SDK_PID 0x0009

static bool with_keyboard;

static tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    // composite device with IAD (needed for CDC)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0110,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

const uint8_t *tud_descriptor_device_cb(void) {
    desc_device.idVendor  = with_keyboard ? USB_VID : SDK_VID;
    desc_device.idProduct = with_keyboard ? USB_PID : SDK_PID;
    return (const uint8_t *)&desc_device;
}

static const uint8_t desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

enum { ITF_CDC = 0, ITF_CDC_DATA, ITF_HID, ITF_COUNT };

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x82
#define EPNUM_HID       0x83

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_TOTAL_LEN, 0, 250),
    TUD_CDC_DESCRIPTOR(ITF_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    TUD_HID_DESCRIPTOR(ITF_HID, 5, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report), EPNUM_HID, 8, 1),
};

#define CONSOLE_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static const uint8_t desc_console_only[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONSOLE_TOTAL_LEN, 0, 250),
    TUD_CDC_DESCRIPTOR(ITF_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return with_keyboard ? desc_configuration : desc_console_only;
}

static const char *string_desc[] = {
    NULL,                       // 0: language
    "1mhz.de",                  // 1: manufacturer
    "Pico64 Keyboard",          // 2: product
    NULL,                       // 3: serial = board id
    "Pico64 Console",           // 4: CDC
    "Pico64 C64 Keyboard",      // 5: HID
};

static uint16_t desc_str[33];

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    const char *s;
    size_t n;
    if (index == 0) {
        desc_str[1] = 0x0409;
        n = 1;
    } else {
        if (index >= sizeof(string_desc) / sizeof(string_desc[0])) return NULL;
        if (index == 3) {
            pico_get_unique_board_id_string(serial, sizeof(serial));
            s = serial;
        } else {
            s = string_desc[index];
        }
        n = strlen(s);
        if (n > 32) n = 32;
        for (size_t i = 0; i < n; i++) desc_str[1 + i] = (uint8_t)s[i];
    }
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * n + 2));
    return desc_str;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type,
                           const uint8_t *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)bufsize;   // LEDs ignored
}

// ---------------------------------------------------------------------------

static uint8_t report_mod;
static uint8_t report_keys[6];
static bool    dirty;

void usb_kbd_init(bool keyboard) {
    with_keyboard = keyboard;
    tusb_init();
}

void usb_kbd_set(uint8_t mods, const uint8_t keys[6]) {
    if (mods == report_mod && memcmp(keys, report_keys, 6) == 0) return;
    report_mod = mods;
    memcpy(report_keys, keys, 6);
    dirty = true;
}

void usb_kbd_task(void) {
    tud_task();
    if (!dirty || !tud_hid_ready()) return;
    if (tud_hid_keyboard_report(0, report_mod, report_keys)) dirty = false;
}
