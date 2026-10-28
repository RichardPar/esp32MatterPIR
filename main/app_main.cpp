#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>

#include <clusters/occupancy_sensing/integration.h>

#include "board.h"
#include "pir_sensor.h"

static const char *TAG = "app_main";

#define ABORT_APP_ON_FAILURE(x, ...) \
    do {                             \
        if (!(x)) {                  \
            __VA_ARGS__;             \
            abort();                 \
        }                            \
    } while (0)

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static uint16_t s_occupancy_endpoint_id;

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP address changed");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed");
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
            // Last fabric gone: reopen the commissioning window so the device can be re-paired.
            chip::CommissioningWindowManager &mgr = chip::Server::GetInstance().GetCommissioningWindowManager();
            constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(300);
            if (!mgr.IsCommissioningWindowOpen()) {
                CHIP_ERROR err = mgr.OpenBasicCommissioningWindow(kTimeoutSeconds,
                                                                  chip::CommissioningWindowAdvertisement::kDnssdOnly);
                if (err != CHIP_NO_ERROR) {
                    ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                }
            }
        }
        break;
    default:
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identify: type %u, effect %u", type, effect_id);
    gpio_set_level(BOARD_LED_GPIO, type == identification::START);
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    // The occupancy sensor has no writable attributes the application needs to act on.
    return ESP_OK;
}

static void pir_changed(bool occupied)
{
    gpio_set_level(BOARD_LED_GPIO, occupied);

    // Occupancy is owned by the SDK's code-driven cluster, so set it there rather than via attribute::update().
    // This must run on the Matter thread.
    chip::DeviceLayer::SystemLayer().ScheduleLambda([occupied]() {
        OccupancySensingCluster *cluster =
            OccupancySensing::FindClusterOnEndpoint(s_occupancy_endpoint_id);
        if (cluster == nullptr) {
            ESP_LOGE(TAG, "Occupancy sensing cluster not found on endpoint %u", s_occupancy_endpoint_id);
            return;
        }
        cluster->SetOccupancy(occupied);
    });
}

extern "C" void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    gpio_reset_pin(BOARD_LED_GPIO);
    gpio_set_direction(BOARD_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_LED_GPIO, 0);

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    occupancy_sensor::config_t sensor_config;
    sensor_config.occupancy_sensing.occupancy_sensor_type =
        chip::to_underlying(OccupancySensing::OccupancySensorTypeEnum::kPir);
    sensor_config.occupancy_sensing.occupancy_sensor_type_bitmap =
        chip::to_underlying(OccupancySensing::OccupancySensorTypeBitmap::kPir);
    sensor_config.occupancy_sensing.feature_flags =
        cluster::occupancy_sensing::feature::passive_infrared::get_id();

    endpoint_t *endpoint = occupancy_sensor::create(node, &sensor_config, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create occupancy sensor endpoint"));

    s_occupancy_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Occupancy sensor created with endpoint_id %d", s_occupancy_endpoint_id);

    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    ESP_ERROR_CHECK(pir_sensor_init(pir_changed));

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::wifi_register_commands();
    esp_matter::console::factoryreset_register_commands();
    esp_matter::console::init();
#endif
}
