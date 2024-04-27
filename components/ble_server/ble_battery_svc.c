#include <host/ble_gatt.h>
#include "ble_battery_svc.h"
#include "ble_common.h"
#include <os/os_mbuf.h>

#ifndef CONFIG_AT_BATTERY_MODEL
#define CONFIG_AT_BATTERY_MODEL "Unknown"
#endif

#ifndef CONFIG_AT_BATTERY_VENDOR
#define CONFIG_AT_BATTERY_VENDOR "Unknown"
#endif

uint16_t ble_server_battery_model_name_handle;
uint16_t ble_server_battery_manuf_name_handle;
uint16_t ble_server_battery_level_handle;
static const char ble_server_battery_model_name[] = CONFIG_AT_BATTERY_MODEL;
static const char ble_server_battery_manuf_name[] = CONFIG_AT_BATTERY_VENDOR;

static int ble_server_handle_battery_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->chr == NULL) {
    return 0;
  }
  if (attr_handle == ble_server_battery_model_name_handle) {
    ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_battery_model_name, sizeof(ble_server_battery_model_name)));
  }
  if (attr_handle == ble_server_battery_manuf_name_handle) {
    ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_battery_manuf_name, sizeof(ble_server_battery_manuf_name)));
  }
  if (attr_handle == ble_server_battery_level_handle) {
    uint8_t level;
    ERROR_CHECK_CALLBACK(at_sensors_get_battery_level(&level, global_ble_server.sensors));
    ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &level, sizeof(level)));
  }
  return 0;
}

void ble_battery_send_updates() {
  uint8_t battery_level;
  esp_err_t batter_level_code = ESP_ERR_NOT_SUPPORTED;
  if (ble_server_has_subscription(ble_server_battery_level_handle)) {
    batter_level_code = at_sensors_get_battery_level(&battery_level, global_ble_server.sensors);
  }
  if (batter_level_code == ESP_OK) {
    ble_solar_send_update(ble_server_battery_level_handle, &battery_level, sizeof(battery_level));
  }
}

static const struct ble_gatt_svc_def ble_battery_items[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SERVER_BATTERY_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[])
            {{
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_MODEL_NAME_UUID),
                 .access_cb = ble_server_handle_battery_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_battery_model_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_MANUFACTURER_NAME_UUID),
                 .access_cb = ble_server_handle_battery_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_battery_manuf_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_BATTERY_LEVEL_UUID),
                 .access_cb = ble_server_handle_battery_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_battery_level_handle
             },
             {
                 0
             }}
    },
    {
        0
    }
};

esp_err_t ble_battery_svc_register() {
  ERROR_CHECK(ble_gatts_count_cfg(ble_battery_items));
  ERROR_CHECK(ble_gatts_add_svcs(ble_battery_items));
  return ESP_OK;
}
