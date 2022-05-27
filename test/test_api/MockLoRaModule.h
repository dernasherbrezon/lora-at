#ifndef MockLoRaModule_h
#define MockLoRaModule_h

#include <LoRaModule.h>

class MockLoRaModule : public LoRaModule {
 public:
  int16_t startLoraRx(LoraState *request);
  LoRaFrame *loop();
  void stopRx();
  bool isReceivingData();
  int16_t loraTx(uint8_t *data, size_t dataLength, LoraState *request);

  int16_t rxCode = ERR_NONE;
  std::vector<LoRaFrame *> expectedFrames;
  bool receiving = false;
  int16_t txCode = ERR_NONE;
  int currentFrameIndex = 0;

};

#endif