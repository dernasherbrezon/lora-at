#ifndef DeepSleepHandler_h
#define DeepSleepHandler_h

#include <Preferences.h>
#include <stdint.h>

class DeepSleepHandler {
 public:
  DeepSleepHandler();
  bool init(uint64_t deepSleepPeriodMicros, uint64_t inactivityTimeoutMicros);
  void handleInactive(bool resetInactiveTimer);
  bool isDeepSleepWakeup();
  void enterDeepSleep(uint64_t deepSleepRequestedMillis);

 private:
  uint64_t deepSleepPeriodMicros = 0;
  uint64_t inactivityTimeoutMicros = 0;
  uint64_t lastActiveTimeMicros = 0;
  Preferences preferences;
};

#endif