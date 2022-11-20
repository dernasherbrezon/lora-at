#include "BatteryVoltage.h"

#include <Arduino.h>

#ifndef PIN_BATTERY_VOLTAGE
#define PIN_BATTERY_VOLTAGE 0
#endif

#ifndef MAX_BATTERY_VOLTAGE
#define MAX_BATTERY_VOLTAGE = 4.2
#endif

#ifndef MIN_BATTERY_VOLTAGE
#define MIN_BATTERY_VOLTAGE = 3.1
#endif

#ifndef BATTERY_R1
#define BATTERY_R1 = 3000
#endif

#ifndef BATTERY_R2
#define BATTERY_R2 = 10000
#endif

int readVoltage(uint8_t *result) {
  if (PIN_BATTERY_VOLTAGE == 0) {
    return -1;
  }
  // TODO should it be configured? Any timeout needed?
  pinMode(PIN_BATTERY_VOLTAGE, INPUT);
  uint16_t rawValue = analogRead(PIN_BATTERY_VOLTAGE);
  float voltageDividerIn = (float)rawValue * (BATTERY_R1 + BATTERY_R2) / BATTERY_R2;
  *result = (uint8_t)((voltageDividerIn - MIN_BATTERY_VOLTAGE) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100);
  return 0;
}