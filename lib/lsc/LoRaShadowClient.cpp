#include "LoRaShadowClient.h"

#include <BLEDevice.h>
#include <esp32-hal-log.h>

LoRaShadowClient::LoRaShadowClient() {
  if (!preferences.begin("lora-at", true)) {
    return;
  }
  uint8_t addressBytes[6];
  preferences.getBytes("address", addressBytes, sizeof(addressBytes));
  //FIXME at some point
  auto size = 18;
	char *res = (char*)malloc(size);
	snprintf(res, size, "%02x:%02x:%02x:%02x:%02x:%02x", addressBytes[0], addressBytes[1], addressBytes[2], addressBytes[3], addressBytes[4], addressBytes[5]);
	std::string ret(res);
	free(res);  
  this->address = new BLEAddress(res);
  preferences.end();
}

bool LoRaShadowClient::connect() {
  return false;
}

bool LoRaShadowClient::init(uint8_t *address, size_t address_len) {
  BLEDevice::init("lora-at");
  BLEClient *client = BLEDevice::createClient();
  this->address = new BLEAddress(address);
  if (!client->connect(*this->address)) {
    return false;
  }
  BLERemoteService *service = client->getService("3f5f0b4d-e311-4921-b29d-936afb8734cc");
  if (service == NULL) {
    log_i("can't find lora-at BLE service");
    return false;
  }

  if (!preferences.begin("lora-at", false)) {
    return false;
  }
  preferences.putBytes("address", address, address_len);
  preferences.end();
  return true;
}