#pragma once

#include <driver/gpio.h>

// MH-ET LIVE ESP32 MiniKit (ESP32-WROOM-32, 4MB flash)
#define BOARD_PIR_GPIO          GPIO_NUM_16   // PIR output (HC-SR501 / AM312), active high
#define BOARD_LED_GPIO          GPIO_NUM_2    // On-board blue LED, active high

#define PIR_DEBOUNCE_MS         50
#define PIR_STARTUP_STABLE_MS   250

// Device sits ~1 m from the AP; full 20 dBm is unnecessary. Valid range 2..20 dBm.
// ESP32 quantises to its power table: 8 dBm reads back as 7 dBm.
#define WIFI_MAX_TX_POWER_DBM   8
