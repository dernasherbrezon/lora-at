#ifndef LORA_AT_BLE_CLIENT_H
#define LORA_AT_BLE_CLIENT_H

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>
#include <nimble/ble.h>

typedef struct {
  SemaphoreHandle_t semaphore;
  esp_err_t semaphore_result;
  uint16_t conn_handle;
  bool service_found;
  uint16_t start_handle;
  uint16_t end_handle;

} ble_client_t;

esp_err_t ble_client_create(uint16_t app_id, ble_client_t **client);

esp_err_t ble_client_connect(ble_addr_t *bt_address, ble_client_t *client);

void ble_client_destroy(ble_client_t *client);

#endif //LORA_AT_BLE_CLIENT_H
