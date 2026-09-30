// Pico64 Keyboard – BLE keyboard service (compatible with the BT-64 BLE keyboard service v1)
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
//
// Everything here runs in the BTstack context (single threaded):
// ATT callbacks put key states into a queue, a BTstack timer applies them to
// the matrix and holds every state for at least KEY_HOLD_MS so the KERNAL
// (keyboard scan every 1/60 s) never misses a short tap.

#include <stdio.h>
#include <string.h>

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include "ble_service.h"
#include "matrix.h"
#include "textfeed.h"
#include "pico64.h"                 // generated from pico64.gatt

#define PROTOCOL_VERSION 1
#define CAPS_FULL_MATRIX 0x01

#define FLAG_SHIFT   0x01
#define FLAG_CMDR    0x02
#define FLAG_CTRL    0x04
#define FLAG_RESTORE 0x08
#define KEY_NONE     0xFF

#define TEXT_FIRST 0x01
#define TEXT_LAST  0x02

#define ATT_APP_ERROR_BUSY     0x80
#define ATT_APP_ERROR_OVERFLOW 0x81

// A pressed key is held longer than a release: the KERNAL scans every 1/60 s
// (16.7 ms), 60 ms guarantees at least three complete scans.
#define KEY_HOLD_MS      60
#define RELEASE_HOLD_MS  30
#define TEXT_HOLD_MS     40
#define QUEUE_LEN    32

#define H_KEY    ATT_CHARACTERISTIC_C64B0002_B1E6_4A64_9C64_6B7E3F1A2D00_01_VALUE_HANDLE
#define H_TEXT   ATT_CHARACTERISTIC_C64B0003_B1E6_4A64_9C64_6B7E3F1A2D00_01_VALUE_HANDLE
#define H_INFO   ATT_CHARACTERISTIC_C64B0004_B1E6_4A64_9C64_6B7E3F1A2D00_01_VALUE_HANDLE
#define H_MATRIX ATT_CHARACTERISTIC_C64B0005_B1E6_4A64_9C64_6B7E3F1A2D00_01_VALUE_HANDLE

// ---------------------------------------------------------------------------
// Advertising: flags, name, 128-bit service UUID (same UUID as the BT-64)

static const uint8_t adv_data[] = {
    2,  BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    7,  BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'P','i','c','o','6','4',
    // C64B0001-B1E6-4A64-9C64-6B7E3F1A2D00, little endian
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
    0x00, 0x2D, 0x1A, 0x3F, 0x7E, 0x6B, 0x64, 0x9C, 0x64, 0x4A, 0xE6, 0xB1, 0x01, 0x00, 0x4B, 0xC6,
};
_Static_assert(sizeof(adv_data) <= 31, "adv_data too big");

// ---------------------------------------------------------------------------
// State

static hci_con_handle_t client = HCI_CON_HANDLE_INVALID;
static btstack_packet_callback_registration_t hci_cb;
static btstack_timer_source_t hold_timer;
static btstack_timer_source_t led_timer;
static bool holding;
static bool debug_log = true;            // key log on the USB console ("d" toggles)

static bool state_empty(const kb_state_t *s) {
    for (int i = 0; i < 8; i++) if (s->cols[i]) return false;
    return !s->restore;
}

static kb_state_t queue[QUEUE_LEN];
static uint8_t q_head, q_tail;

static char     text_buf[TEXTFEED_MAX];
static uint16_t text_len;
static bool     text_collecting;

static bool q_empty(void) { return q_head == q_tail; }

static void q_push(const kb_state_t *s) {
    uint8_t next = (uint8_t)((q_head + 1) % QUEUE_LEN);
    if (next == q_tail) {                      // full: drop the oldest
        q_tail = (uint8_t)((q_tail + 1) % QUEUE_LEN);
    }
    queue[q_head] = *s;
    q_head = next;
}

// ---------------------------------------------------------------------------
// Scheduler: apply one state, hold it, then take the next one

static void schedule(void);

static void hold_done(btstack_timer_source_t *ts) {
    (void)ts;
    holding = false;
    schedule();
}

static void schedule(void) {
    if (holding) return;
    kb_state_t s;
    uint32_t hold;
    if (textfeed_busy()) {
        if (!textfeed_next(&s)) return;
        hold = TEXT_HOLD_MS;
    } else if (!q_empty()) {
        s = queue[q_tail];
        q_tail = (uint8_t)((q_tail + 1) % QUEUE_LEN);
        hold = state_empty(&s) ? RELEASE_HOLD_MS : KEY_HOLD_MS;
        if (debug_log) {
            printf("%8lu ms  apply ", (unsigned long)to_ms_since_boot(get_absolute_time()));
            for (int i = 0; i < 8; i++) printf("%02x", s.cols[i]);
            printf("%s\n", s.restore ? " +RESTORE" : "");
        }
    } else {
        return;
    }
    matrix_apply(&s);
    holding = true;
    btstack_run_loop_set_timer(&hold_timer, hold);
    btstack_run_loop_add_timer(&hold_timer);
}

static void post_state(const kb_state_t *s) {
    if (textfeed_busy()) return;               // the text feed owns the keyboard
    q_push(s);
    schedule();
}

static void add_flags(kb_state_t *s, uint8_t flags) {
    if (flags & FLAG_SHIFT) kb_press(s, KB_LSHIFT_COL, KB_LSHIFT_ROW);
    if (flags & FLAG_CMDR)  kb_press(s, KB_CMDR_COL,   KB_CMDR_ROW);
    if (flags & FLAG_CTRL)  kb_press(s, KB_CTRL_COL,   KB_CTRL_ROW);
    if (flags & FLAG_RESTORE) s->restore = true;
}

static void release_everything(void) {
    textfeed_abort();
    text_collecting = false;
    q_head = q_tail = 0;
    kb_state_t none = {0};
    q_push(&none);
    schedule();
}

// ---------------------------------------------------------------------------
// ATT

static int handle_text(const uint8_t *buffer, uint16_t size) {
    if (size < 1) return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
    uint8_t flags = buffer[0];
    if (flags & TEXT_FIRST) { text_len = 0; text_collecting = true; }
    if (!text_collecting) return ATT_ERROR_REQUEST_NOT_SUPPORTED;
    if (text_len + (size - 1) > TEXTFEED_MAX) { text_collecting = false; return ATT_APP_ERROR_OVERFLOW; }
    if ((flags & TEXT_LAST) && textfeed_busy()) return ATT_APP_ERROR_BUSY;   // client retries

    memcpy(&text_buf[text_len], &buffer[1], size - 1);
    text_len = (uint16_t)(text_len + size - 1);

    if (flags & TEXT_LAST) {
        text_collecting = false;
        textfeed_start(text_buf, text_len);
        printf("text feed: %u chars\n", text_len);
        schedule();
    }
    return 0;
}

static int att_write_cb(hci_con_handle_t con, uint16_t handle, uint16_t mode,
                        uint16_t offset, uint8_t *buffer, uint16_t size) {
    (void)con;
    if (mode != ATT_TRANSACTION_MODE_NONE || offset != 0)
        return ATT_ERROR_REQUEST_NOT_SUPPORTED;

    kb_state_t s = {0};
    switch (handle) {
    case H_KEY:
        if (size != 2) return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
        if (debug_log)
            printf("%8lu ms  BLE key flags=%02x key=%02x\n",
                   (unsigned long)to_ms_since_boot(get_absolute_time()), buffer[0], buffer[1]);
        add_flags(&s, buffer[0]);
        if (buffer[1] != KEY_NONE) kb_press(&s, (buffer[1] >> 3) & 7, buffer[1] & 7);
        post_state(&s);
        return 0;
    case H_MATRIX:
        if (size != 9) return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
        add_flags(&s, buffer[0]);
        for (int i = 0; i < 8; i++) s.cols[i] |= buffer[1 + i];
        post_state(&s);
        return 0;
    case H_TEXT:
        return handle_text(buffer, size);
    default:
        return 0;
    }
}

static uint16_t att_read_cb(hci_con_handle_t con, uint16_t handle, uint16_t offset,
                            uint8_t *buffer, uint16_t size) {
    (void)con;
    if (handle == H_INFO) {
        uint8_t info[3] = { PROTOCOL_VERSION, textfeed_busy() ? 1 : 0, CAPS_FULL_MATRIX };
        return att_read_callback_handle_blob(info, sizeof(info), offset, buffer, size);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Events, advertising, status LED

static void start_advertising(void) {
    bd_addr_t null_addr = {0};
    // 100 ms interval (units of 0.625 ms)
    gap_advertisements_set_params(160, 160, 0, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(sizeof(adv_data), (uint8_t *)adv_data);
    gap_advertisements_enable(1);
}

static void packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)channel; (void)size;
    if (type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
            bd_addr_t addr;
            gap_local_bd_addr(addr);
            printf("Pico64 ready, BLE address %s\n", bd_addr_to_str(addr));
            start_advertising();
        }
        break;
    case ATT_EVENT_CONNECTED:
        client = att_event_connected_get_handle(packet);
        printf("app connected\n");
        // Ask for a short connection interval: 15-30 ms (units of 1.25 ms), no latency,
        // 4 s timeout - within Apple's accessory guidelines. iOS otherwise often uses
        // 30 ms or more, which makes key presses feel late.
        gap_request_connection_parameter_update(client, 12, 24, 0, 400);
        break;
    case ATT_EVENT_DISCONNECTED:
        client = HCI_CON_HANDLE_INVALID;
        printf("app disconnected - releasing all keys\n");
        release_everything();
        break;
    case HCI_EVENT_LE_META:
        switch (hci_event_le_meta_get_subevent_code(packet)) {
        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
            printf("connected, interval %u.%02u ms\n",
                   hci_subevent_le_connection_complete_get_conn_interval(packet) * 125 / 100,
                   hci_subevent_le_connection_complete_get_conn_interval(packet) * 125 % 100);
            break;
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V1:
            printf("connected, interval %u.%02u ms\n",
                   hci_subevent_le_enhanced_connection_complete_v1_get_conn_interval(packet) * 125 / 100,
                   hci_subevent_le_enhanced_connection_complete_v1_get_conn_interval(packet) * 125 % 100);
            break;
        case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
            printf("connection interval now %u.%02u ms\n",
                   hci_subevent_le_connection_update_complete_get_conn_interval(packet) * 125 / 100,
                   hci_subevent_le_connection_update_complete_get_conn_interval(packet) * 125 % 100);
            break;
        default:
            break;
        }
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        client = HCI_CON_HANDLE_INVALID;
        gap_advertisements_enable(1);
        break;
    default:
        break;
    }
}

// LED: blinking while waiting for the app, on while connected
static void led_tick(btstack_timer_source_t *ts) {
    static bool on;
    on = (client != HCI_CON_HANDLE_INVALID) ? true : !on;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
    btstack_run_loop_set_timer(ts, 500);
    btstack_run_loop_add_timer(ts);
}

// ---------------------------------------------------------------------------

void ble_service_init(void) {
    l2cap_init();
    sm_init();
    att_server_init(profile_data, att_read_cb, att_write_cb);
    att_server_register_packet_handler(packet_handler);

    hci_cb.callback = &packet_handler;
    hci_add_event_handler(&hci_cb);

    hold_timer.process = &hold_done;
    led_timer.process = &led_tick;
    btstack_run_loop_set_timer(&led_timer, 500);
    btstack_run_loop_add_timer(&led_timer);
}

void ble_service_toggle_debug(void) {
    debug_log = !debug_log;
    printf("key log %s\n", debug_log ? "on" : "off");
}

bool ble_service_connected(void) {
    return client != HCI_CON_HANDLE_INVALID;
}

bool ble_service_type_text(const char *text, size_t len) {
    if (!textfeed_start(text, len)) return false;
    schedule();
    return true;
}
