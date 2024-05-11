#include <host/ble_hs_mbuf.h>
#include <host/ble_gatt.h>
#include <string.h>
#include <esp_log.h>
#include "ble_common.h"

const ble_presentation_format_t voltage_format = {
    .gatt_format = 0x06, //uint16
    .exponent = 0x03, // millivolt
    .gatt_unit = 0x2728, //volt
    .gatt_namespace = 0x00,
    .gatt_nsdesc = 0x0000
};

const ble_presentation_format_t power_format = {
    .gatt_format = 0x06, //uint16
    .exponent = 0x03,
    .gatt_unit = 0x2726, //milliwatt
    .gatt_namespace = 0x00,
    .gatt_nsdesc = 0x0000
};

const ble_presentation_format_t current_format = {
    .gatt_format = 0x0E, //int16
    .exponent = 0x03, //milliampere
    .gatt_unit = 0x2704, //ampere
    .gatt_namespace = 0x00,
    .gatt_nsdesc = 0x0000
};

const ble_presentation_format_t celsius_format = {
    .gatt_format = 0x0C, //sint8
    .exponent = 0x00,
    .gatt_unit = 0x272F, //degree Celsius
    .gatt_namespace = 0x00,
    .gatt_nsdesc = 0x0000
};

const ble_presentation_format_t utf8_string_format = {
    .gatt_format = 0x19, //utf8
    .exponent = 0x00,
    .gatt_unit = 0x2700, //unitless
    .gatt_namespace = 0x00,
    .gatt_nsdesc = 0x0000
};

ble_server_t global_ble_server;

bool ble_server_has_subscription(uint16_t handle) {
  for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
    if (!global_ble_server.client[i].active) {
      continue;
    }
    for (int j = 0; j < BLE_SERVER_MAX_SUBSCRIPTIONS; j++) {
      if (global_ble_server.client[i].subsription_handles[j] == handle) {
        return true;
      }
    }
  }
  return false;
}

bool ble_server_has_client_subscription(ble_server_client_t *client, uint16_t handle) {
  for (int i = 0; i < BLE_SERVER_MAX_SUBSCRIPTIONS; i++) {
    if (client->subsription_handles[i] == handle) {
      return true;
    }
  }
  return false;
}

void ble_server_send_update(uint16_t handle, void *data, size_t data_length) {
  for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
    if (!global_ble_server.client[i].active) {
      continue;
    }
    if (!ble_server_has_client_subscription(&global_ble_server.client[i], handle)) {
      continue;
    }
    memcpy(global_ble_server.temp_buffer, data, data_length);
    struct os_mbuf *txom = ble_hs_mbuf_from_flat(global_ble_server.temp_buffer, data_length);
    ESP_LOGD("ble_server", "sending %d bytes to connection %d on handle %d", data_length, global_ble_server.client[i].conn_id, handle);
    ble_gatts_notify_custom(global_ble_server.client[i].conn_id, handle, txom);
  }
}

bool ble_server_is_authorized(uint16_t conn_id) {
  for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
    if (global_ble_server.client[i].conn_id != conn_id) {
      continue;
    }
    return global_ble_server.client[i].authorized;
  }
  return false;
}