#ifndef LORA_AT_BLE_SERVER_H
#define LORA_AT_BLE_SERVER_H

#include <at_sensors.h>
#include <esp_err.h>

typedef struct ble_server_t ble_server;

esp_err_t ble_server_create(at_sensors *sensors, ble_server **server);

void ble_server_send_notifications(ble_server *server);

void ble_server_destroy(ble_server *server);


#endif //LORA_AT_BLE_SERVER_H
