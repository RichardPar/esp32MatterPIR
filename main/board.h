#pragma once

#include <driver/gpio.h>

// MH-ET LIVE ESP32 MiniKit (ESP32-WROOM-32, 4MB flash)
#define BOARD_PIR_GPIO          GPIO_NUM_16   // PIR output (HC-SR501 / AM312), active high
#define BOARD_LED_GPIO          GPIO_NUM_2    // On-board blue LED, active high

#define PIR_DEBOUNCE_MS         50
