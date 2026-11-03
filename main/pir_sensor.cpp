#include "pir_sensor.h"
#include "board.h"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "pir";

static TaskHandle_t s_task;
static pir_sensor_cb_t s_cb;

static void IRAM_ATTR pir_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_task, &woken);
    portYIELD_FROM_ISR(woken);
}

// Sample until the level has held for stable_ms.
static bool read_stable_level(uint32_t stable_ms)
{
    bool level = gpio_get_level(BOARD_PIR_GPIO);
    uint32_t stable = 0;
    while (stable < stable_ms) {
        vTaskDelay(pdMS_TO_TICKS(10));
        bool now = gpio_get_level(BOARD_PIR_GPIO);
        if (now == level) {
            stable += 10;
        } else {
            level = now;
            stable = 0;
        }
    }
    return level;
}

static void pir_task(void *arg)
{
    // The line can glitch for >100 ms at power-up; don't report until it settles.
    bool reported = read_stable_level(PIR_STARTUP_STABLE_MS);
    ESP_LOGI(TAG, "Initial state: %s", reported ? "occupied" : "unoccupied");
    s_cb(reported);

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Wait for the line to settle, then sample once.
        vTaskDelay(pdMS_TO_TICKS(PIR_DEBOUNCE_MS));
        ulTaskNotifyTake(pdTRUE, 0);

        bool level = gpio_get_level(BOARD_PIR_GPIO);
        if (level != reported) {
            reported = level;
            ESP_LOGI(TAG, "Motion %s", level ? "detected" : "cleared");
            s_cb(level);
        }
    }
}

esp_err_t pir_sensor_init(pir_sensor_cb_t cb)
{
    s_cb = cb;

    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << BOARD_PIR_GPIO;
    io.mode = GPIO_MODE_INPUT;
    io.pull_down_en = GPIO_PULLDOWN_ENABLE;   // keep the line low if the sensor is unplugged
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.intr_type = GPIO_INTR_ANYEDGE;
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "gpio_config failed");

    if (xTaskCreate(pir_task, "pir", 3072, nullptr, 5, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {   // already installed is fine
        return err;
    }
    return gpio_isr_handler_add(BOARD_PIR_GPIO, pir_isr, nullptr);
}
