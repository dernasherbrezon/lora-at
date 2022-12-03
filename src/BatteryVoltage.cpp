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
#define BATTERY_R1 = 4700
#endif

#ifndef BATTERY_R2
#define BATTERY_R2 = 10000
#endif

int readVoltage(uint8_t *result) {
  if (PIN_BATTERY_VOLTAGE == 0) {
    *result = 0xFF;
    return 0;
  }
  // TODO should it be configured? Any timeout needed?
  pinMode(PIN_BATTERY_VOLTAGE, INPUT);
  uint16_t rawValue = analogRead(PIN_BATTERY_VOLTAGE);
  float voltageDividerIn = (float)(rawValue * 3.3 / 4095) * (BATTERY_R1 + BATTERY_R2) / BATTERY_R2;
  // not connected
  if( voltageDividerIn < MIN_BATTERY_VOLTAGE ) {
    *result = 0xFF;
  }
  *result = (uint8_t)((voltageDividerIn - MIN_BATTERY_VOLTAGE) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100);
  return 0;
}