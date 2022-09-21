#ifndef DeepSleepHandler_h
#define DeepSleepHandler_h

#include <Preferences.h>
#include <stdint.h>

class DeepSleepHandler {
 public:
  DeepSleepHandler();
  bool init(uint64_t deepSleepPeriod, uint64_t inactivityTimeout);
  void loop();

 private:
  uint64_t deepSleepPeriod = 0;
  uint64_t inactivityTimeout = 0;
  uint64_t lastActiveTime = 0;
  Preferences preferences;
};

#endif