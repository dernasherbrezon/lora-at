#include "ble_solar_svc.h"
#include "ble_common.h"
#include <host/ble_gatt.h>
#include <os/os_mbuf.h>
#include "sdkconfig.h"

#ifndef CONFIG_AT_SOLAR_MODEL
#define CONFIG_AT_SOLAR_MODEL "Unknown"
#endif

#ifndef CONFIG_AT_SOLAR_VENDOR
#define CONFIG_AT_SOLAR_VENDOR "Unknown"
#endif

#ifndef CONFIG_AT_SOLAR_NOMVOLTAGE
#define CONFIG_AT_SOLAR_NOMVOLTAGE "Unknown"
#endif

#ifndef CONFIG_AT_SOLAR_RATED_POWER
#define CONFIG_AT_SOLAR_RATED_POWER "Unknown"
#endif

#ifndef CONFIG_AT_SOLAR_MATERIAL
#define CONFIG_AT_SOLAR_MATERIAL "Unknown"
#endif

uint16_t ble_server_solar_model_name_handle;
static const char ble_server_solar_model_name[] = CONFIG_AT_SOLAR_MODEL;
uint16_t ble_server_solar_manuf_name_handle;
static const char ble_server_solar_manuf_name[] = CONFIG_AT_SOLAR_VENDOR;
uint16_t ble_server_solar_voltage_handle;
uint16_t ble_server_solar_current_handle;
static const char ble_server_nomvoltage_name[] = "Nominal Voltage";
uint16_t ble_server_solar_nomvoltage_handle;
static const char ble_server_nomvoltage[] = CONFIG_AT_SOLAR_NOMVOLTAGE;
static const char ble_server_solar_rated_power_name[] = "Rated Power";
uint16_t ble_server_solar_rated_power_handle;
static const char ble_server_solar_rated_power[] = CONFIG_AT_SOLAR_RATED_POWER;
uint16_t ble_server_solar_power_handle;
uint16_t ble_server_solar_material_handle;
static const char ble_server_solar_material[] = CONFIG_AT_SOLAR_MATERIAL;
static const char ble_server_solar_material_name[] = "Material";

static int ble_server_handle_solar_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    if (attr_handle == ble_server_solar_model_name_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_solar_model_name, sizeof(ble_server_solar_model_name)));
    }
    if (attr_handle == ble_server_solar_manuf_name_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_solar_manuf_name, sizeof(ble_server_solar_manuf_name)));
    }
    if (attr_handle == ble_server_solar_nomvoltage_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_nomvoltage, sizeof(ble_server_nomvoltage)));
    }
    if (attr_handle == ble_server_solar_material_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_solar_material, sizeof(ble_server_solar_material)));
    }
    if (attr_handle == ble_server_solar_rated_power_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_solar_rated_power, sizeof(ble_server_solar_rated_power)));
    }
    if (attr_handle == ble_server_solar_power_handle) {
      uint32_t solar_power;
      ERROR_CHECK_CALLBACK(at_sensors_get_solar_power(&solar_power, global_ble_server.sensors));
      solar_power = htole32(solar_power);
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &solar_power, 3)); // uint24 according to BLE spec
    }
    if (attr_handle == ble_server_solar_voltage_handle) {
      uint16_t voltage;
      ERROR_CHECK_CALLBACK(at_sensors_get_solar_voltage(&voltage, global_ble_server.sensors));
      voltage = htole16(voltage);
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &voltage, sizeof(voltage)));
    }
    if (attr_handle == ble_server_solar_current_handle) {
      int16_t current;
      ERROR_CHECK_CALLBACK(at_sensors_get_solar_current(&current, global_ble_server.sensors));
      current = htole16(current);
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &current, sizeof(current)));
    }
  }
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
    if (ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT)) == 0) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &utf8_string_format, sizeof(utf8_string_format)));
    }
    if (ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION)) == 0) {
      attr_handle -= 2;
      if (attr_handle == ble_server_solar_nomvoltage_handle) {
        ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_nomvoltage_name, sizeof(ble_server_nomvoltage_name)));
      }
      if (attr_handle == ble_server_solar_material_handle) {
        ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_solar_material_name, sizeof(ble_server_solar_material_name)));
      }
      if (attr_handle == ble_server_solar_rated_power_handle) {
        ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_solar_rated_power_name, sizeof(ble_server_solar_rated_power_name)));
      }
    }
  }
  return 0;
}

void ble_solar_send_updates() {
  uint16_t solar_voltage;
  esp_err_t solar_voltage_code = ESP_ERR_NOT_SUPPORTED;
  if (ble_server_has_subscription(ble_server_solar_voltage_handle)) {
    solar_voltage_code = at_sensors_get_solar_voltage(&solar_voltage, global_ble_server.sensors);
  }
  if (solar_voltage_code == ESP_OK) {
    solar_voltage = htole16(solar_voltage);
    ble_server_send_update(ble_server_solar_voltage_handle, &solar_voltage, sizeof(solar_voltage));
  }
  int16_t solar_current;
  esp_err_t solar_current_code = ESP_ERR_NOT_SUPPORTED;
  if (ble_server_has_subscription(ble_server_solar_current_handle)) {
    solar_current_code = at_sensors_get_solar_current(&solar_current, global_ble_server.sensors);
  }
  if (solar_current_code == ESP_OK) {
    solar_current = htole16(solar_current);
    ble_server_send_update(ble_server_solar_current_handle, &solar_current, sizeof(solar_current));
  }
  uint32_t solar_power;
  esp_err_t solar_power_code = ESP_ERR_NOT_SUPPORTED;
  if (ble_server_has_subscription(ble_server_solar_power_handle)) {
    solar_power_code = at_sensors_get_solar_power(&solar_power, global_ble_server.sensors);
  }
  if (solar_power_code == ESP_OK) {
    solar_power = htole32(solar_power);
    ble_server_send_update(ble_server_solar_power_handle, &solar_power, 3);
  }
}

static const struct ble_gatt_svc_def ble_solar_items[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0x8d, 0xe1, 0x2b, 0x32, 0x41, 0xe, 0x45, 0xf9, 0xaa, 0x6d, 0x2f, 0xec, 0x81, 0xb9, 0xa5, 0x95),
        .characteristics = (struct ble_gatt_chr_def[])
            {{
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_MODEL_NAME_UUID),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_solar_model_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_MANUFACTURER_NAME_UUID),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_solar_manuf_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_VOLTAGE_UUID),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_solar_voltage_handle
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0x39, 0xb7, 0x14, 0x2, 0x4e, 0xa1, 0x4e, 0x64, 0xaf, 0xf, 0xc, 0x27, 0x9b, 0x8b, 0x8f, 0x3),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_solar_nomvoltage_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_solar_service,
                      },
                      {
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_solar_service,
                      },
                      {
                          0
                      }}
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0xef, 0xdc, 0xe8, 0xe4, 0xe5, 0x47, 0x42, 0x8, 0xa3, 0x66, 0xc7, 0x3d, 0xe9, 0x29, 0x7, 0xe7),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_solar_material_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_solar_service,
                      },
                      {
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_solar_service,
                      },
                      {
                          0
                      }}
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0xef, 0x76, 0xaf, 0xd8, 0xa7, 0x6a, 0x41, 0xc9, 0xa0, 0x80, 0x15, 0xfd, 0xe6, 0x8e, 0xcc, 0xb1),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_solar_rated_power_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_solar_service,
                      },
                      {
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_solar_service,
                      },
                      {
                          0
                      }}
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_POWER_UUID),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_solar_power_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_ELECTRIC_CURRENT_UUID),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_solar_current_handle
             },
             {
                 0
             }}
    },
    {
        0
    }
};

esp_err_t ble_solar_svc_register() {
  ERROR_CHECK(ble_gatts_count_cfg(ble_solar_items));
  ERROR_CHECK(ble_gatts_add_svcs(ble_solar_items));
  return ESP_OK;
}