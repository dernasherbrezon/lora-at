#ifndef LoRaShadowClient_h
#define LoRaShadowClient_h

#include <BLEDevice.h>
#include <LoRaFrame.h>
#include <LoRaModule.h>
#include <Preferences.h>
#include <stdint.h>

class LoRaShadowClient {
 public:
  LoRaShadowClient();
  bool init(uint8_t *address, size_t address_len);
  void loadRequest(ObservationRequest *state);
  void sendBatteryLevel(uint16_t level);
  void sendData(LoRaFrame *frame);

 private:
  BLEClient *client = NULL;
  BLERemoteService *service = NULL;
  BLERemoteCharacteristic *req = NULL;
  BLERemoteCharacteristic *battery = NULL;
  BLEAddress *address = NULL;
  Preferences preferences;

  uint64_t htonll(uint64_t x);
  uint64_t ntohll(uint64_t x);
};

#endif