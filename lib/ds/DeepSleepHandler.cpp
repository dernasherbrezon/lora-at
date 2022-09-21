#include "DeepSleepHandler.h"

#include <esp32-hal-log.h>
#include <esp_timer.h>

//FIXME preserve memory during deep sleep
//FIXME correctly wake up

DeepSleepHandler::DeepSleepHandler() {
  if (!preferences.begin("lora-at", true)) {
    return;
  }
  this->deepSleepPeriod = preferences.getULong64("period");
  this->inactivityTimeout = preferences.getULong64("inactivity");
  preferences.end();
}

bool DeepSleepHandler::init(uint64_t deepSleepPeriodMillis, uint64_t inactivityTimeout) {
  if (!preferences.begin("lora-at", false)) {
    return false;
  }
  this->deepSleepPeriod = deepSleepPeriodMillis;
  this->inactivityTimeout = inactivityTimeout;
  preferences.putULong64("period", deepSleepPeriod);
  preferences.putULong64("inactivity", inactivityTimeout);
  preferences.end();
  if (esp_sleep_enable_timer_wakeup(deepSleepPeriodMillis * 1000) != ESP_OK) {
    return false;
  }
  this->lastActiveTime = esp_timer_get_time();
  return true;
}

void DeepSleepHandler::loop() {
  if (deepSleepPeriod == 0) {
    // not configured
    return;
  }
  if (esp_timer_get_time() - this->lastActiveTime < this->inactivityTimeout) {
    return;
  }
  log_i("go into deep sleep mode");
  Serial.flush();
  esp_deep_sleep_start();
}
