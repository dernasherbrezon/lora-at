#include "display.h"

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

#define logo_width 32
#define logo_height 32
static uint8_t logo_bits[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x38,
    0x00,
    0x00,
    0x00,
    0xFC,
    0x00,
    0x00,
    0x00,
    0xFE,
    0x01,
    0x00,
    0x00,
    0xFC,
    0x01,
    0x00,
    0x00,
    0xF8,
    0x07,
    0x00,
    0x00,
    0xF0,
    0x07,
    0x00,
    0x00,
    0xE0,
    0x0F,
    0x00,
    0x00,
    0xC0,
    0x3F,
    0x00,
    0x00,
    0x80,
    0x7F,
    0x00,
    0x00,
    0x00,
    0x3F,
    0x01,
    0x00,
    0x00,
    0xBE,
    0x07,
    0x00,
    0x00,
    0xDC,
    0x07,
    0x00,
    0x00,
    0xE8,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF8,
    0x0F,
    0x00,
    0x00,
    0xFE,
    0x27,
    0x00,
    0x00,
    0xFE,
    0x3B,
    0x00,
    0x00,
    0xFC,
    0xFD,
    0x00,
    0x00,
    0x7C,
    0xFE,
    0x01,
    0x00,
    0xF0,
    0xFC,
    0x01,
    0x01,
    0x41,
    0xF8,
    0x07,
    0xB2,
    0x01,
    0xF0,
    0x07,
    0x22,
    0x03,
    0xE0,
    0x1F,
    0x66,
    0x00,
    0xC0,
    0x1F,
    0xC4,
    0x01,
    0x80,
    0x3F,
    0x1C,
    0x02,
    0x00,
    0x7F,
    0x18,
    0x00,
    0x00,
    0x3E,
    0x70,
    0x00,
    0x00,
    0x1C,
    0xC0,
    0x01,
    0x00,
    0x08,
    0x00,
    0x01,
    0x00,
    0x00,
};

esp_err_t lora_at_display_create(lora_at_display_t **display) {
  lora_at_display_t *result = malloc(sizeof(lora_at_display_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
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
  *display = result;
  return ESP_OK;
}

esp_err_t lora_at_display_set_status(const char *status, lora_at_display_t *display) {
  ssd1306_clear_screen(display->ssd1306_dev, 0x00);
  ssd1306_draw_bitmap(display->ssd1306_dev, 0, 0, logo_bits, logo_width, logo_height);
  ssd1306_draw_string(display->ssd1306_dev, logo_width + 5, 5, (const uint8_t *) "lora-at", 16, 1);
  char status_buffer[20] = {0};
  sprintf(status_buffer, "status: %s", status);
  ssd1306_draw_string(display->ssd1306_dev, logo_width + 5, 21, (const uint8_t *) status_buffer, 16, 1);
  return ssd1306_refresh_gram(display->ssd1306_dev);
//  this->display->clear();
//  this->display->drawXbm(0, 0, logo_width, logo_height, logo_bits);
//  this->display->setFont(ArialMT_Plain_10);
//  this->display->setTextAlignment(TEXT_ALIGN_LEFT);
//  this->display->drawString(logo_width + 5, 5, "lora-at");
//  if (status != NULL) {
//    this->display->drawString(logo_width + 5, 21, "status: " + String(status));
//  } else {
//    this->display->drawString(logo_width + 5, 21, "status:");
//  }
//  this->display->display();
}

void lora_at_display_destroy(lora_at_display_t *display) {
  if (display == NULL) {
    return;
  }
  ssd1306_delete(display->ssd1306_dev);
  i2c_driver_delete(I2C_MASTER_NUM);
  free(display);
}