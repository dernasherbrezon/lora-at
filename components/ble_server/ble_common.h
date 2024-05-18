#ifndef LORA_AT_BLE_COMMON_H
#define LORA_AT_BLE_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <at_sensors.h>
#include <sx127x.h>
#include <at_config.h>
#include "sx127x_util.h"

// 1 battery
// 3 solar panel
// 2 sx127x
#define BLE_SERVER_MAX_SUBSCRIPTIONS 6

// services
#define BLE_SERVER_BATTERY_SERVICE 0x180F

// characteristics
#define BLE_SERVER_BATTERY_LEVEL_UUID 0x2A19
#define BLE_SERVER_MODEL_NAME_UUID 0x2A24
#define BLE_SERVER_SOFTWARE_VERSION_UUID 0x2A28
#define BLE_SERVER_MANUFACTURER_NAME_UUID 0x2A29
#define BLE_SERVER_TEMPERATURE_UUID 0x2A6E
#define BLE_SERVER_ELECTRIC_CURRENT_UUID 0x2AEE
#define BLE_SERVER_POWER_UUID 0x2B05
#define BLE_SERVER_VOLTAGE_UUID 0x2B18

// descriptions
#define BLE_SERVER_USER_DESCRIPTION 0x2901
#define BLE_SERVER_PRESENTATION_FORMAT 0x2904

#define ERROR_CHECK(x)        \
  do {                        \
    int __err_rc = (x); \
    if (__err_rc != 0) {      \
      return __err_rc;        \
    }                         \
  } while (0)

#define ERROR_CHECK_CALLBACK(x)        \
  do {                        \
    int __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      return BLE_ATT_ERR_INSUFFICIENT_RES;        \
    }                         \
  } while (0)

#define ERROR_CHECK_RESPONSE(x)        \
  do {                        \
    int rc = (x); \
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;      \
  } while (0)

typedef struct __attribute__((packed)) {
  uint8_t gatt_format;
  int8_t exponent;
  uint16_t gatt_unit;
  uint8_t gatt_namespace;
  uint16_t gatt_nsdesc;
} ble_presentation_format_t;

typedef struct {
  bool active;
  bool authorized;
  uint16_t conn_id;
  uint16_t subsription_handles[BLE_SERVER_MAX_SUBSCRIPTIONS];
  uint16_t mtu;
} ble_server_client_t;

typedef struct {
  uint8_t temp_buffer[CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU];
  at_sensors *sensors;
  sx127x_wrapper *device;
  lora_at_config_t *config;
  ble_server_client_t client[CONFIG_BT_NIMBLE_MAX_CONNECTIONS];
} ble_server_t;

extern ble_server_t global_ble_server;
extern const ble_presentation_format_t voltage_format;
extern const ble_presentation_format_t power_format;
extern const ble_presentation_format_t current_format;
extern const ble_presentation_format_t celsius_format;
extern const ble_presentation_format_t utf8_string_format;

bool ble_server_has_subscription(uint16_t handle);

bool ble_server_has_client_subscription(ble_server_client_t *client, uint16_t handle);

bool ble_server_is_authorized(uint16_t conn_id);

void ble_server_send_update(uint16_t handle, void *data, size_t data_length);

#endif //LORA_AT_BLE_COMMON_H
