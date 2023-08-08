#ifndef LoRaShadowClient_h
#define LoRaShadowClient_h

#include <BLEDevice.h>
#include <Preferences.h>
#include <lora_util.h>
#include <stdint.h>

class LoRaShadowClient {
 public:
  LoRaShadowClient();
  bool init(uint8_t *address, size_t address_len);
  void loadRequest(rx_request *state);
  void sendBatteryLevel(uint8_t level);
  void sendData(lora_frame *frame);
  void getAddress(uint8_t **address, size_t *address_len);

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