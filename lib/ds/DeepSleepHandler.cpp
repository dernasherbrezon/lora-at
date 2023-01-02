#include "DeepSleepHandler.h"

#include <driver/rtc_io.h>
#include <esp32-hal-log.h>
#include <esp_timer.h>

#ifndef PIN_DI0
#define PIN_DI0 26
#endif

DeepSleepHandler::DeepSleepHandler() {
  if (!preferences.begin("lora-at", true)) {
    return;
  }
  this->deepSleepPeriodMicros = preferences.getULong64("period");
  this->inactivityTimeoutMicros = preferences.getULong64("inactivity");
  preferences.end();
}

DeepSleepHandler::DeepSleepHandler(uint64_t deepSleepPeriodMicros, uint64_t inactivityTimeoutMicros) {
  this->deepSleepPeriodMicros = deepSleepPeriodMicros;
  this->inactivityTimeoutMicros = inactivityTimeoutMicros;
}

bool DeepSleepHandler::init(uint64_t deepSleepPeriodMicros, uint64_t inactivityTimeoutMicros) {
  if (!preferences.begin("lora-at", false)) {
    return false;
  }
  this->deepSleepPeriodMicros = deepSleepPeriodMicros;
  this->inactivityTimeoutMicros = inactivityTimeoutMicros;
  preferences.putULong64("period", deepSleepPeriodMicros);
  preferences.putULong64("inactivity", inactivityTimeoutMicros);
  preferences.end();
  return true;
}

void DeepSleepHandler::enterDeepSleep(uint64_t deepSleepRequestedMicros) {
  if (this->deepSleepPeriodMicros == 0) {
    // not configured
    return;
  }
  uint64_t deepSleepTime;
  if (deepSleepRequestedMicros == 0 || this->deepSleepPeriodMicros < deepSleepRequestedMicros) {
    deepSleepTime = this->deepSleepPeriodMicros;
  } else {
    deepSleepTime = deepSleepRequestedMicros;
  }
  log_i("entering deep sleep mode for %" PRIu64 " seconds", deepSleepTime / 1000000);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(deepSleepTime);
  esp_deep_sleep_start();
}

void DeepSleepHandler::enterRxDeepSleep(uint64_t deepSleepRequestedMicros) {
  rtc_gpio_set_direction((gpio_num_t)PIN_DI0, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_en((gpio_num_t)PIN_DI0);
  log_i("entering rx deep sleep for %" PRIu64 " seconds or first packet", deepSleepRequestedMicros / 1000000);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(deepSleepRequestedMicros);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_DI0, RISING);
  esp_deep_sleep_start();
}

bool DeepSleepHandler::isDeepSleepRequired(bool resetInactiveTimer) {
  if (this->deepSleepPeriodMicros == 0) {
    // not configured
    return false;
  }
  uint64_t currentTime = esp_timer_get_time();

  if (resetInactiveTimer) {
    this->lastActiveTimeMicros = currentTime;
  }

  if (currentTime - this->lastActiveTimeMicros < this->inactivityTimeoutMicros) {
    return false;
  }

  log_i("not active for %d seconds. going into deep sleep", this->inactivityTimeoutMicros / 1000000);
  return true;
}
