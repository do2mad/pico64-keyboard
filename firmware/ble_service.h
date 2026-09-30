// Pico64 Keyboard – BLE keyboard service (compatible with the BT-64 BLE keyboard service v1)
// Copyright (c) 2026 Martin Oswald (do2mad, 1mhz.de) - SPDX-License-Identifier: MIT
#ifndef PICO64_BLE_SERVICE_H
#define PICO64_BLE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

void ble_service_init(void);          // before hci_power_control(HCI_POWER_ON)
bool ble_service_connected(void);

/** Key log on the USB console on/off. */
void ble_service_toggle_debug(void);
bool ble_service_type_text(const char *text, size_t len);   // for the USB console

#endif
