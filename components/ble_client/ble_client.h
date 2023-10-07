#ifndef LORA_AT_BLE_CLIENT_H
#define LORA_AT_BLE_CLIENT_H

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct ble_client_t ble_client;

esp_err_t ble_client_create(uint16_t app_id, ble_client **client);

esp_err_t ble_client_connect(uint8_t *address, ble_client *client);

void ble_client_destroy(ble_client *client);

#endif //LORA_AT_BLE_CLIENT_H
