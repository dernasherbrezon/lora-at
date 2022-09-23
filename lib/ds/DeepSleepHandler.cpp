#include "DeepSleepHandler.h"

#include <esp32-hal-log.h>
#include <esp_timer.h>

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
  return true;
}

void DeepSleepHandler::enterDeepSleep(uint64_t deepSleepRequestedMillis) {
  uint64_t deepSleepTime;
  if (deepSleepRequestedMillis == 0 || this->deepSleepPeriod < deepSleepRequestedMillis) {
    deepSleepTime = this->deepSleepPeriod / 1000;
  } else {
    deepSleepTime = deepSleepRequestedMillis / 1000;
  }
  log_i("entering deep sleep mode for %d seconds", deepSleepTime);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(deepSleepTime * 1000);
  esp_deep_sleep_start();
}

void DeepSleepHandler::handleInactive(bool resetInactiveTimer) {
  if (this->deepSleepPeriod == 0) {
    // not configured
    return;
  }
  uint64_t currentTime = esp_timer_get_time();

  if (resetInactiveTimer) {
    this->lastActiveTime = currentTime;
  }

  if (currentTime - this->lastActiveTime < this->inactivityTimeout) {
    return;
  }

  log_i("not active for %d seconds. going into deep sleep", this->inactivityTimeout / 1000);
  this->enterDeepSleep(0);
}

bool DeepSleepHandler::isDeepSleepWakeup() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}
