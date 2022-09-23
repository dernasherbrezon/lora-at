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
  void sendData(LoRaFrame *frame);

 private:
  BLEClient *client;
  BLERemoteService *service;
  BLERemoteCharacteristic *req;
  BLEAddress *address;
  Preferences preferences;
};

#endif