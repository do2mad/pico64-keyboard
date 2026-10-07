// Pico64 Keyboard – Bluepad32 configuration (replaces ESP-IDF "menuconfig")
// Based on the Bluepad32 Pico W example (public domain).
#ifndef PICO64_SDKCONFIG_H
#define PICO64_SDKCONFIG_H

#define CONFIG_BLUEPAD32_MAX_DEVICES 4          // keyboards + gamepads at the same time
#define CONFIG_BLUEPAD32_MAX_ALLOWLIST 4
#define CONFIG_BLUEPAD32_GAP_SECURITY 1
#define CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT 1  // BLE keyboards and gamepads too

#define CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#define CONFIG_TARGET_PICO_W

// 0 = none, 1 = errors, 2 = info (pairing messages on the USB console)
#define CONFIG_BLUEPAD32_LOG_LEVEL 1

#endif
