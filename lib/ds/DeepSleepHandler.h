#ifndef DeepSleepHandler_h
#define DeepSleepHandler_h

#include <Preferences.h>
#include <stdint.h>

class DeepSleepHandler {
 public:
  DeepSleepHandler();
  DeepSleepHandler(uint64_t deepSleepPeriodMicros, uint64_t inactivityTimeoutMicros);
  bool init(uint64_t deepSleepPeriodMicros, uint64_t inactivityTimeoutMicros);
  bool isDeepSleepRequired(bool resetInactiveTimer);
  void enterDeepSleep(uint64_t deepSleepRequestedMillis);
  void enterRxDeepSleep(uint64_t deepSleepRequestedMicros);
  uint64_t deepSleepPeriodMicros = 0;
  uint64_t inactivityTimeoutMicros = 0;

 private:
  uint64_t lastActiveTimeMicros = 0;
  Preferences preferences;
};

#endif