#include "deep_sleep.h"
#include <esp_log.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

//TODO source from sx127x_util? Or from the main config?
#ifndef CONFIG_PIN_DIO0
#define CONFIG_PIN_DIO0 26
#endif

#define ERROR_CHECK(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      ESP_LOGE(TAG, "unable to initialize %s: %s", y, esp_err_to_name(__err_rc));                        \
      return;        \
    }                         \
  } while (0)

static const char *TAG = "lora-at";

void deep_sleep_enter(uint64_t micros_to_wait) {
  esp_sleep_enable_timer_wakeup(micros_to_wait);
  ESP_LOGI(TAG, "entering deep sleep mode for %" PRIu64 " seconds", (micros_to_wait / 1000000));
  esp_deep_sleep_start();
}

void deep_sleep_rx_enter(uint64_t micros_to_wait) {
  ERROR_CHECK("rtc_gpio_set_direction", rtc_gpio_set_direction((gpio_num_t) CONFIG_PIN_DIO0, RTC_GPIO_MODE_INPUT_ONLY));
  ERROR_CHECK("rtc_gpio_pulldown_en", rtc_gpio_pulldown_en((gpio_num_t) CONFIG_PIN_DIO0));
  // max wait is ~400 days https://github.com/espressif/esp-idf/blob/42cce06704a24b01721cd34920f25b2e48b88c55/components/esp_hw_support/port/esp32s2/rtc_time.c#L205
  ERROR_CHECK("esp_sleep_enable_timer_wakeup", esp_sleep_enable_timer_wakeup(micros_to_wait));
  ERROR_CHECK("esp_sleep_enable_ext0_wakeup", esp_sleep_enable_ext0_wakeup((gpio_num_t) CONFIG_PIN_DIO0, 1));
  ESP_LOGI(TAG, "entering rx deep sleep for %" PRIu64 " seconds or first packet", (micros_to_wait / 1000000));
  esp_deep_sleep_start();
}
