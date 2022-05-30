#include "MockLoRaModule.h"

int16_t MockLoRaModule::startLoraRx(LoraState *request) {
  return rxCode;
}

LoRaFrame *MockLoRaModule::loop() {
  if (expectedFrames.empty() || currentFrameIndex >= expectedFrames.size()) {
    return NULL;
  }
  LoRaFrame *result = expectedFrames[currentFrameIndex];
  currentFrameIndex++;
  return result;
}

void MockLoRaModule::stopRx() {
  // do nothing
}

bool MockLoRaModule::isReceivingData() {
  return receiving;
}
int16_t MockLoRaModule::loraTx(uint8_t *data, size_t dataLength, LoraState *request) {
  return txCode;
}