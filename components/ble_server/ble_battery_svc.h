#ifndef LORA_AT_BLE_BATTERY_SVC_H
#define LORA_AT_BLE_BATTERY_SVC_H

#include <esp_err.h>

esp_err_t ble_battery_svc_register();

void ble_battery_send_updates();

#endif //LORA_AT_BLE_BATTERY_SVC_H
