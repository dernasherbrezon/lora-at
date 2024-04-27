#ifndef LORA_AT_BLE_SERVER_H
#define LORA_AT_BLE_SERVER_H

#include <at_sensors.h>
#include <esp_err.h>

esp_err_t ble_server_create(at_sensors *sensors);

void ble_server_send_updates();

#endif //LORA_AT_BLE_SERVER_H
