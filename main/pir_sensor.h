#pragma once

#include <esp_err.h>

typedef void (*pir_sensor_cb_t)(bool occupied);

// Configure the PIR GPIO and start the monitor task. The callback runs in the
// monitor task context, once per debounced state change, and once at start-up
// with the initial state.
esp_err_t pir_sensor_init(pir_sensor_cb_t cb);
