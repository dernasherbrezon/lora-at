#include "DeepSleepHandler.h"

#include <esp32-hal-log.h>
#include <esp_timer.h>

DeepSleepHandler::DeepSleepHandler() {
  if (!preferences.begin("lora-at", true)) {
    return;
  }
  this->deepSleepPeriodMicros = preferences.getULong64("period");
  this->inactivityTimeoutMicros = preferences.getULong64("inactivity");
  preferences.end();
  if (!this->isDeepSleepWakeup()) {
    this->lastActiveTimeMicros = esp_timer_get_time();
  }
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
  log_i("entering deep sleep mode for %d seconds", deepSleepTime / 1000000);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(deepSleepTime);
  esp_deep_sleep_start();
}

void DeepSleepHandler::handleInactive(bool resetInactiveTimer) {
  if (this->deepSleepPeriodMicros == 0) {
    // not configured
    return;
  }
  uint64_t currentTime = esp_timer_get_time();

  if (resetInactiveTimer) {
    this->lastActiveTimeMicros = currentTime;
  }

  if (currentTime - this->lastActiveTimeMicros < this->inactivityTimeoutMicros) {
    return;
  }

  log_i("not active for %d seconds. going into deep sleep", this->inactivityTimeoutMicros / 1000000);
  this->enterDeepSleep(0);
}

bool DeepSleepHandler::isDeepSleepWakeup() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}