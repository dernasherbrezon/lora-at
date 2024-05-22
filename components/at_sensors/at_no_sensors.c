#include "at_sensors.h"

esp_err_t at_sensors_init(at_sensors **dev) {
  *dev = NULL;
  return ESP_OK;
}

esp_err_t at_sensors_get_solar_voltage(uint16_t *bus_voltage, at_sensors *dev) {
  *bus_voltage = 0xFFFF; // According to BLE spec this is "value is not known"
  return ESP_OK;
}

esp_err_t at_sensors_get_solar_current(int16_t *current, at_sensors *dev) {
  *current = 0xFFFF; // According to BLE spec this is "value is not known"
  return ESP_OK;
}

esp_err_t at_sensors_get_solar_power(uint32_t *power, at_sensors *dev) {
  *power = 0xFFFFFFFF; // According to BLE spec this is "value is not known"
  return ESP_OK;
}

esp_err_t at_sensors_get_battery_voltage(uint16_t *bus_voltage, at_sensors *dev) {
  *bus_voltage = 0xFFFF; // According to BLE spec this is "value is not known"
  return ESP_OK;
}

esp_err_t at_sensors_get_battery_current(int16_t *current, at_sensors *dev) {
  *current = 0xFFFF; // According to BLE spec this is "value is not known"
  return ESP_OK;
}

esp_err_t at_sensors_get_battery_level(uint8_t *level, at_sensors *dev) {
  *level = 0;
  return ESP_OK;
}

void at_sensors_destroy(at_sensors *dev) {
  //do nmothing
}
