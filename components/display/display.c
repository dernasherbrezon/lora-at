#include "display.h"
#include <ssd1306.h>

#ifndef CONFIG_PIN_OLED_SDL
#define CONFIG_PIN_OLED_SDL 22        /*!&lt; gpio number for I2C master clock */
#endif

#ifndef CONFIG_PIN_OLED_SDA
#define CONFIG_PIN_OLED_SDA 21        /*!&lt; gpio number for I2C master data  */
#endif

#ifndef CONFIG_PIN_OLED_RESET
#define  CONFIG_PIN_OLED_RESET -1
#endif

#define I2C_MASTER_NUM I2C_NUM_1    /*!&lt; I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000   /*!&lt; I2C master clock frequency */

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      return __err_rc;        \
    }                         \
  } while (0)

struct lora_at_display_t {
  ssd1306_handle_t ssd1306_dev;
};

#define logo_width 32
#define logo_height 32
static uint8_t logo_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00,
    0x7f, 0x00, 0x00, 0x00, 0x3f, 0x80, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x0f, 0xe0, 0x00, 0x00,
    0x07, 0xf0, 0x00, 0x00, 0x03, 0xf8, 0x00, 0x00, 0x01, 0xfc, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00,
    0x00, 0x79, 0xe0, 0x00, 0x00, 0x33, 0xf0, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x0f, 0xf0, 0x00,
    0x00, 0x2f, 0xf0, 0x00, 0x00, 0x7f, 0xe8, 0x00, 0x00, 0x3f, 0xdc, 0x00, 0x00, 0x3f, 0xbe, 0x00,
    0x00, 0x3e, 0x3f, 0x00, 0x00, 0x0e, 0x3f, 0x80, 0x09, 0x02, 0x1f, 0xc0, 0xcd, 0x80, 0x0f, 0xe0,
    0x44, 0xc0, 0x07, 0xf0, 0x66, 0x00, 0x03, 0xf8, 0x23, 0x80, 0x01, 0xfc, 0x30, 0xc0, 0x00, 0xfe,
    0x18, 0x00, 0x00, 0x7c, 0x0e, 0x00, 0x00, 0x38, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

esp_err_t lora_at_display_create(lora_at_display **display) {
  struct lora_at_display_t *result = malloc(sizeof(struct lora_at_display_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->ssd1306_dev = NULL;
  *display = result;
  return ESP_OK;
}

esp_err_t lora_at_display_start(lora_at_display *result) {
  if (result->ssd1306_dev != NULL) {
    return ESP_OK;
  }
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = (gpio_num_t) CONFIG_PIN_OLED_SDA;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_io_num = (gpio_num_t) CONFIG_PIN_OLED_SDL;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
  conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

  ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
  ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));

  result->ssd1306_dev = ssd1306_create(I2C_MASTER_NUM, SSD1306_I2C_ADDRESS);
  ERROR_CHECK(ssd1306_refresh_gram(result->ssd1306_dev));
  ssd1306_clear_screen(result->ssd1306_dev, 0x00);
  return ESP_OK;
}

esp_err_t lora_at_display_stop(lora_at_display *display) {
  if (display->ssd1306_dev == NULL) {
    return ESP_OK;
  }
  ssd1306_delete(display->ssd1306_dev);
  esp_err_t result = i2c_driver_delete(I2C_MASTER_NUM);
  display->ssd1306_dev = NULL;
  return result;
}

esp_err_t lora_at_display_set_status(const char *status, lora_at_display *display) {
  ssd1306_clear_screen(display->ssd1306_dev, 0x00);
  ssd1306_draw_bitmap(display->ssd1306_dev, 0, 0, logo_bits, logo_width, logo_height);
  ssd1306_draw_string(display->ssd1306_dev, logo_width + 5, 5, (const uint8_t *) "lora-at", 14, 1);
  char status_buffer[20] = {0};
  sprintf(status_buffer, "status: %s", status);
  ssd1306_draw_string(display->ssd1306_dev, logo_width + 5, 21, (const uint8_t *) status_buffer, 14, 1);
  return ssd1306_refresh_gram(display->ssd1306_dev);
}

void lora_at_display_destroy(lora_at_display *display) {
  if (display == NULL) {
    return;
  }
  if (display->ssd1306_dev != NULL) {
    lora_at_display_stop(display);
  }
  free(display);
}