#ifndef LORA_AT_BLE_SX127X_SVC_H
#define LORA_AT_BLE_SX127X_SVC_H

#include <esp_err.h>

esp_err_t ble_sx127x_svc_register();

void ble_sx127x_send_updates();

#endif //LORA_AT_BLE_SX127X_SVC_H
