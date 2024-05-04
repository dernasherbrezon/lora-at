#ifndef LORA_AT_BLE_CLIENT_H
#define LORA_AT_BLE_CLIENT_H

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>
#include <sx127x_util.h>

typedef struct ble_client_t ble_client;

#pragma pack(push, 1)
typedef struct {
  uint8_t protocol_version;
  int8_t rssi;
  int8_t sx127x_raw_temperature;
  uint16_t solar_voltage;
  int16_t solar_current;
  uint16_t battery_voltage;
  int16_t battery_current;
} ble_client_status;
#pragma pack(pop)

esp_err_t ble_client_create(uint8_t *address, ble_client **client);

esp_err_t ble_client_connect(uint8_t *address, ble_client *client);

esp_err_t ble_client_disconnect(ble_client *client);

esp_err_t ble_client_load_request(lora_config_t **request, ble_client *client);

esp_err_t ble_client_send_frame(sx127x_frame_t *frame, ble_client *client);

esp_err_t ble_client_get_rssi(ble_client *client, int8_t *rssi);

esp_err_t ble_client_send_status(ble_client_status *status, ble_client *client);

void ble_client_destroy(ble_client *client);

#endif //LORA_AT_BLE_CLIENT_H
