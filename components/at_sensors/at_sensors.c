#include "at_sensors.h"
#include "sdkconfig.h"
#include <ina219.h>

#define SHUNT_RESISTOR_MILLI_OHM 100
#define MAX_CURRENT 5
#define I2C_PORT 0

#ifndef CONFIG_SOLAR_I2C_INA219_ADDR
#define CONFIG_SOLAR_I2C_INA219_ADDR -1
#endif

#ifndef CONFIG_BATTERY_I2C_INA219_ADDR
#define CONFIG_BATTERY_I2C_INA219_ADDR -1
#endif

#ifndef CONFIG_I2C_MASTER_SDA
#define CONFIG_I2C_MASTER_SDA 21
#endif

#ifndef CONFIG_I2C_MASTER_SCL
#define CONFIG_I2C_MASTER_SCL 22
#endif

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      return __err_rc;        \
    }                         \
  } while (0)

esp_err_t at_sensors_get_data(uint8_t addr, uint16_t *bus_voltage, int16_t *current) {
  ina219_t dev;
  ERROR_CHECK(ina219_init_desc(&dev, addr, I2C_PORT, CONFIG_I2C_MASTER_SDA, CONFIG_I2C_MASTER_SCL));
  ERROR_CHECK(ina219_init(&dev));
  ERROR_CHECK(ina219_configure(&dev, INA219_BUS_RANGE_16V, INA219_GAIN_0_125, INA219_RES_12BIT_1S, INA219_RES_12BIT_1S, INA219_MODE_CONT_SHUNT_BUS));
  ERROR_CHECK(ina219_calibrate(&dev, (float) MAX_CURRENT, (float) SHUNT_RESISTOR_MILLI_OHM / 1000.0f));

  float result_bus_voltage;
  float result_current;
  ERROR_CHECK(ina219_get_bus_voltage(&dev, &result_bus_voltage));
  ERROR_CHECK(ina219_get_current(&dev, &result_current));
  *bus_voltage = (uint16_t) (result_bus_voltage * 1000);
  *current = (int16_t) (result_current * 1000);
  return ESP_OK;
}

esp_err_t at_sensors_get_solar(uint16_t *bus_voltage, int16_t *current) {
  if (CONFIG_SOLAR_I2C_INA219_ADDR == -1) {
    *bus_voltage = 0;
    *current = 0;
    return ESP_OK;
  }
  return at_sensors_get_data(CONFIG_SOLAR_I2C_INA219_ADDR, bus_voltage, current);
}

esp_err_t at_sensors_get_battery(uint16_t *bus_voltage, int16_t *current) {
  if (CONFIG_BATTERY_I2C_INA219_ADDR == -1) {
    *bus_voltage = 0;
    *current = 0;
    return ESP_OK;
  }
  return at_sensors_get_data(CONFIG_BATTERY_I2C_INA219_ADDR, bus_voltage, current);
}
