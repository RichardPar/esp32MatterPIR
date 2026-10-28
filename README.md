# esp32MatterPIR

Matter-over-Wi-Fi occupancy sensor. PIR input on GPIO16, MH-ET LIVE ESP32 MiniKit (ESP32-WROOM-32, 4MB).

Vendor `CaveEnterprise`, product `PIR`. Device type Occupancy Sensor (0x0107) on endpoint 1, PIR feature.

## Hardware

| PIR | MiniKit |
|-----|---------|
| VCC | 5V (HC-SR501) / 3V3 (AM312) |
| OUT | IO16 |
| GND | GND |

- IO16 is configured input, internal pull-down, any-edge interrupt, 50 ms debounce.
- IO2 (blue LED) mirrors IO16. Also driven by Identify.
- No user button on this board. Factory reset is via the console.

> **Caution:** some PIR modules false-trigger from nearby 2.4 GHz RF. Keep the sensor a few cm from the ESP32
> antenna. When testing, cover the dome fully: a partial cover still lets the sensor see movement.

## Build

ESP-IDF v6.1. `espressif/esp_matter` ^1.6.1 comes from the component registry on first configure.

```sh
. ~/.espressif/tools/activate_idf_v6.1.sh
idf.py set-target esp32            # once
idf.py -p /dev/ttyACM0 erase-flash flash monitor
```

Erase on first flash, or after changing the partition table.

## Commissioning

On boot with no fabric, the device advertises over BLE for 15 min (`CONFIG_CHIP_DISCOVERY_TIMEOUT_SECS`).
Reset to reopen the window. Once paired, it doesn't advertise again until the last fabric is removed or the device
is factory reset.

Firmware uses the SDK test DAC and default onboarding payload. The boot log prints the actual codes. Defaults:

- passcode `20202021`, discriminator `3840`
- manual code `34970112332`

```sh
chip-tool pairing ble-wifi 1 <ssid> <psk> 20202021 3840
chip-tool occupancysensing read occupancy 1 1
chip-tool basicinformation read vendor-name 1 0
```

Uncertified test credentials: Apple/Google/SmartThings need developer or test mode.

## Console

UART0 at 115200. `matter help` for the full list.

```
matter device factoryreset
matter onboardingcodes ble
matter wifi connect <ssid> <psk>
```

## Layout

```
main/app_main.cpp          node + endpoint setup, event handling, PIR -> cluster
main/pir_sensor.cpp        GPIO ISR, debounce task
main/board.h               pin map
main/CHIPProjectConfig.h   SDK config overrides (vendor/product name)
partitions.csv             2 x 1.875MB OTA slots, nvs, fctry
sdkconfig.defaults         BLE commissioning, Wi-Fi only, -Os, console
```

## Notes

- Occupancy is owned by the SDK's code-driven `OccupancySensingCluster`. `esp_matter::attribute::update()`
  on 0x0406/0x0000 returns `ESP_ERR_NOT_SUPPORTED` (262). Use `FindClusterOnEndpoint()->SetOccupancy()`, on
  the Matter thread (`SystemLayer().ScheduleLambda`).
- `CHIP_DEVICE_CONFIG_*` macros must go in `CHIPProjectConfig.h`. A `#define` in `main/` doesn't reach the
  SDK build. Wired in through `CONFIG_CHIP_PROJECT_CONFIG`.
- With `CONFIG_NONE_DEVICE_INFO_PROVIDER` the names come from those macros. Moving to factory NVS data
  (`fctry`) overrides them.
- When the last fabric is removed, the device reopens a 300 s DNS-SD commissioning window.
- App image is ~1.47MB, ~22% headroom per OTA slot.
- The build warns about `std::is_trivial` deprecation in SDK headers (GCC 15). It's upstream and harmless.
