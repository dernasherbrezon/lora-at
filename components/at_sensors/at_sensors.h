#ifndef LORA_AT_AT_SENSORS_H
#define LORA_AT_AT_SENSORS_H

#include <esp_err.h>

typedef struct at_sensors_t at_sensors;

esp_err_t at_sensors_init(at_sensors **dev);

esp_err_t at_sensors_get_solar_voltage(uint16_t *bus_voltage, at_sensors *dev);

esp_err_t at_sensors_get_solar_current(int16_t *current, at_sensors *dev);

esp_err_t at_sensors_get_battery_voltage(uint16_t *bus_voltage, at_sensors *dev);

esp_err_t at_sensors_get_battery_current(int16_t *current, at_sensors *dev);

esp_err_t at_sensors_get_battery_level(uint8_t *level, at_sensors *dev);

void at_sensors_destroy(at_sensors *dev);

#endif //LORA_AT_AT_SENSORS_H
