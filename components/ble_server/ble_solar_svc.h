#ifndef LORA_AT_BLE_SOLAR_SVC_H
#define LORA_AT_BLE_SOLAR_SVC_H

#include <esp_err.h>

esp_err_t ble_solar_svc_register();

void ble_solar_send_updates();

#endif //LORA_AT_BLE_SOLAR_SVC_H
