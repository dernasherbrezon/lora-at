#ifndef LORA_AT_AT_SENSORS_H
#define LORA_AT_AT_SENSORS_H

#include <esp_err.h>

esp_err_t at_sensors_init();

esp_err_t at_sensors_get_solar(uint16_t *bus_voltage, int16_t *current);

esp_err_t at_sensors_get_battery(uint16_t *bus_voltage, int16_t *current);

void at_sensors_destroy();

#endif //LORA_AT_AT_SENSORS_H
