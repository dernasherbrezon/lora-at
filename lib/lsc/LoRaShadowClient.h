#ifndef LoRaShadowClient_h
#define LoRaShadowClient_h

#include <Preferences.h>
#include <BLEDevice.h>
#include <stdint.h>

class LoRaShadowClient {
 public:
  LoRaShadowClient();
  bool connect();
  bool init(uint8_t *address, size_t address_len);

 private:
  BLEAddress *address;
  Preferences preferences;
};

#endif