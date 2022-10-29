#include "LoRaShadowClient.h"

#include <BLEDevice.h>
#include <esp32-hal-log.h>

#define SERVICE_UUID "3f5f0b4d-e311-4921-b29d-936afb8734cc"
#define REQUEST_UUID "40d6f70c-5e28-4da4-a99e-c5298d1613fe"

LoRaShadowClient::LoRaShadowClient() {
  if (!preferences.begin("lora-at", true)) {
    return;
  }
  uint8_t addressBytes[6];
  preferences.getBytes("address", addressBytes, sizeof(addressBytes));
  // FIXME at some point
  auto size = 18;
  char *res = (char *)malloc(size);
  snprintf(res, size, "%02x:%02x:%02x:%02x:%02x:%02x", addressBytes[0], addressBytes[1], addressBytes[2], addressBytes[3], addressBytes[4], addressBytes[5]);
  std::string ret(res);
  free(res);
  this->address = new BLEAddress(res);
  preferences.end();
}

bool LoRaShadowClient::init(uint8_t *address, size_t address_len) {
  this->address = new BLEAddress(address);
  if (this->client == NULL) {
    BLEDevice::init("lora-at");
    BLEClient *tempClient = BLEDevice::createClient();
    if (!tempClient->connect(*this->address)) {
      return false;
    }
    this->client = tempClient;
  }
  if (this->service == NULL) {
    this->service = this->client->getService(SERVICE_UUID);
    if (this->service == NULL) {
      log_i("can't find lora-at BLE service");
      return false;
    }
  }

  if (!preferences.begin("lora-at", false)) {
    return false;
  }
  preferences.putBytes("address", address, address_len);
  preferences.end();
  return true;
}

void LoRaShadowClient::loadRequest(ObservationRequest *state) {
  if (this->client == NULL) {
    BLEDevice::init("lora-at");
    BLEClient *tempClient = BLEDevice::createClient();
    if (!tempClient->connect(*this->address)) {
      return;
    }
    this->client = tempClient;
  }
  if (this->service == NULL) {
    this->service = this->client->getService(SERVICE_UUID);
    if (this->service == NULL) {
      log_i("can't find lora-at BLE service");
      return;
    }
  }
  if (this->req == NULL) {
    this->req = this->service->getCharacteristic(REQUEST_UUID);
    if (req == NULL) {
      log_i("can't find characteristic");
      return;
    }
  }

  std::string value = req->readValue();
  uint8_t *raw = (uint8_t *)value.data();
  size_t raw_length = value.size();
  if (raw_length == 0) {
    log_i("no observation scheduled");
    memset(state, 0, sizeof(ObservationRequest));
    return;
  }
  memcpy(state, raw, raw_length);
  log_i("observation requested: %f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu", state->freq, state->bw, state->sf, state->cr, state->syncWord, state->power, state->preambleLength, state->gain, state->ldro);
}

void LoRaShadowClient::sendData(LoRaFrame *frame) {
  if (this->client == NULL) {
    BLEDevice::init("lora-at");
    BLEClient *tempClient = BLEDevice::createClient();
    if (!tempClient->connect(*this->address)) {
      return;
    }
    this->client = tempClient;
  }
  if (this->service == NULL) {
    this->service = this->client->getService(SERVICE_UUID);
    if (this->service == NULL) {
      log_i("can't find lora-at BLE service");
      return;
    }
  }
  if (this->req == NULL) {
    this->req = this->service->getCharacteristic(REQUEST_UUID);
    if (req == NULL) {
      log_i("can't find characteristic");
      return;
    }
  }

  size_t length = 0;
  length += 4;  // frame->getFrequencyError();
  length += 4;  // frame->getRssi();
  length += 4;  // frame->getSnr();
  length += 8;  // frame->getTimestamp();
  length += 4;  // frame->getDataLength();
  length += frame->dataLength;

  uint8_t *message = (uint8_t *)malloc(sizeof(uint8_t) * length);
  if (message == NULL) {
    return;
  }

  float frequencyError = frame->frequencyError;
  size_t offset = 0;
  memcpy(message + offset, &frequencyError, sizeof(float));
  float rssi = frame->rssi;
  offset += 4;
  memcpy(message + offset, &rssi, sizeof(float));
  float snr = frame->snr;
  offset += 4;
  memcpy(message + offset, &snr, sizeof(float));
  long timestamp = frame->timestamp;
  offset += 4;
  memcpy(message + offset, &timestamp, sizeof(long));
  size_t dataLength = frame->dataLength;
  offset += 8;
  memcpy(message + offset, &dataLength, sizeof(size_t));
  offset += 4;
  memcpy(message + offset, frame->data, frame->dataLength);

  req->writeValue(message, length, false);
}