#ifndef LORA_AT_BLE_SERVER_H
#define LORA_AT_BLE_SERVER_H

#include <at_sensors.h>
#include <esp_err.h>
#include <sx127x.h>
#include <sx127x_util.h>
#include <at_config.h>

esp_err_t ble_server_create(at_sensors *sensors, sx127x_wrapper *device, lora_at_config_t *config);

void ble_server_send_updates();

void ble_server_send_frame(sx127x_frame_t *frame);

#endif //LORA_AT_BLE_SERVER_H
