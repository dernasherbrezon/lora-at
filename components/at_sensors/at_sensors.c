#include "at_sensors.h"
#include "sdkconfig.h"
#include <string.h>
#include <ina219.h>

#define SHUNT_RESISTOR_MILLI_OHM 100
#define MAX_CURRENT 3.2
#define I2C_PORT 0

#ifndef CONFIG_SOLAR_I2C_INA219_ADDR
#define CONFIG_SOLAR_I2C_INA219_ADDR 0
#endif

#ifndef CONFIG_BATTERY_I2C_INA219_ADDR
#define CONFIG_BATTERY_I2C_INA219_ADDR 0
#endif

#ifndef CONFIG_I2C_MASTER_SDA
#define CONFIG_I2C_MASTER_SDA 21
#endif

#ifndef CONFIG_I2C_MASTER_SCL
#define CONFIG_I2C_MASTER_SCL 22
#endif

#ifndef CONFIG_AT_BATTERY_MIN_VOLTAGE
#define CONFIG_AT_BATTERY_MIN_VOLTAGE 3000
#endif

#ifndef CONFIG_AT_BATTERY_MAX_VOLTAGE
#define CONFIG_AT_BATTERY_MAX_VOLTAGE 4200
#endif

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      return __err_rc;        \
    }                         \
  } while (0)

struct at_sensors_t {
  ina219_t battery;
  ina219_t solar;
};

esp_err_t at_sensors_get_solar_voltage(uint16_t *bus_voltage, at_sensors *dev) {
  float result_bus_voltage;
  ERROR_CHECK(ina219_get_bus_voltage(&dev->solar, &result_bus_voltage));
  *bus_voltage = (uint16_t) (result_bus_voltage * 1000);
  return ESP_OK;
}

esp_err_t at_sensors_get_solar_current(int16_t *current, at_sensors *dev) {
  float result_current;
  ERROR_CHECK(ina219_get_current(&dev->solar, &result_current));
  *current = (int16_t) (result_current * 1000);
  return ESP_OK;
}

esp_err_t at_sensors_get_battery_voltage(uint16_t *bus_voltage, at_sensors *dev) {
  float result_bus_voltage;
  ERROR_CHECK(ina219_get_bus_voltage(&dev->battery, &result_bus_voltage));
  *bus_voltage = (uint16_t) (result_bus_voltage * 1000);
  return ESP_OK;
}

esp_err_t at_sensors_get_battery_current(int16_t *current, at_sensors *dev) {
  float result_current;
  ERROR_CHECK(ina219_get_current(&dev->battery, &result_current));
  *current = (int16_t) (result_current * 1000);
  return ESP_OK;
}

esp_err_t at_sensors_get_battery_level(uint8_t *level, at_sensors *dev) {
  uint16_t voltage;
  ERROR_CHECK(at_sensors_get_battery_voltage(&voltage, dev));
  if (voltage < CONFIG_AT_BATTERY_MIN_VOLTAGE) {
    *level = 0;
    return ESP_OK;
  }
  *level = (uint8_t) ((voltage - CONFIG_AT_BATTERY_MIN_VOLTAGE) / (CONFIG_AT_BATTERY_MAX_VOLTAGE - CONFIG_AT_BATTERY_MIN_VOLTAGE)) * 100;
  return ESP_OK;
}

esp_err_t at_sensors_sensor_init(uint8_t addr, ina219_t *dev) {
  memset(dev, 0, sizeof(ina219_t));
  ERROR_CHECK(ina219_init_desc(dev, addr, I2C_PORT, CONFIG_I2C_MASTER_SDA, CONFIG_I2C_MASTER_SCL));
  ERROR_CHECK(ina219_init(dev));
  ERROR_CHECK(ina219_configure(dev, INA219_BUS_RANGE_16V, INA219_GAIN_0_125, INA219_RES_12BIT_1S, INA219_RES_12BIT_1S, INA219_MODE_CONT_SHUNT_BUS));
  ERROR_CHECK(ina219_calibrate(dev, (float) MAX_CURRENT, (float) SHUNT_RESISTOR_MILLI_OHM / 1000.0f));
  return ESP_OK;
}

esp_err_t at_sensors_init(at_sensors **dev) {
  struct at_sensors_t *result = malloc(sizeof(struct at_sensors_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  ERROR_CHECK(at_sensors_sensor_init(CONFIG_BATTERY_I2C_INA219_ADDR, &result->battery));
  ERROR_CHECK(at_sensors_sensor_init(CONFIG_SOLAR_I2C_INA219_ADDR, &result->solar));

  *dev = result;
  return ESP_OK;
}

void at_sensors_destroy(at_sensors *dev) {
  if (dev == NULL) {
    return;
  }
  free(dev);
}
