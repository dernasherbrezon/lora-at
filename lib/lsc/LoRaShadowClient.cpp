#include "LoRaShadowClient.h"

#include <BLEDevice.h>
#include <esp32-hal-log.h>
#include <lwip/sockets.h>

#define SERVICE_UUID "3f5f0b4d-e311-4921-b29d-936afb8734cc"
#define REQUEST_UUID "40d6f70c-5e28-4da4-a99e-c5298d1613fe"
#define STATUS_UUID "5b53256e-76d2-4259-b3aa-15b5b4cfdd32"
#define OBSERVATION_SIZE 40

//FIXME load address from preferences or supplied from parameter
// this should save nvm load during deep sleep
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

void LoRaShadowClient::sendBatteryLevel(uint8_t level) {
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
  if (this->battery == NULL) {
    this->battery = this->service->getCharacteristic(STATUS_UUID);
    if (battery == NULL) {
      log_i("can't find battery characteristic");
      return;
    }
  }

  int8_t rssi = (int8_t)this->client->getRssi();

  size_t messageLength = (sizeof(rssi) + sizeof(level));
  uint8_t *message = (uint8_t *)malloc(sizeof(uint8_t) * messageLength);
  if (message == NULL) {
    return;
  }
  size_t offset = 0;
  memcpy(message + offset, &level, sizeof(level));
  offset += sizeof(level);

  memcpy(message + offset, &rssi, sizeof(rssi));
  offset += sizeof(rssi);

  battery->writeValue(message, messageLength, true);
  free(message);
}

void LoRaShadowClient::loadRequest(rx_request *state) {
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
    memset(state, 0, sizeof(rx_request_t));
    return;
  }
  if (raw_length < OBSERVATION_SIZE) {
    log_i("corrupted message received. expected: %zu", OBSERVATION_SIZE);
    memset(state, 0, sizeof(rx_request_t));
    return;
  }

  size_t offset = 0;

  memcpy(&(state->startTimeMillis), raw + offset, sizeof(state->startTimeMillis));
  state->startTimeMillis = ntohll(state->startTimeMillis);
  offset += sizeof(state->startTimeMillis);

  memcpy(&(state->endTimeMillis), raw + offset, sizeof(state->endTimeMillis));
  state->endTimeMillis = ntohll(state->endTimeMillis);
  offset += sizeof(state->endTimeMillis);

  memcpy(&(state->currentTimeMillis), raw + offset, sizeof(state->currentTimeMillis));
  state->currentTimeMillis = ntohll(state->currentTimeMillis);
  offset += sizeof(state->currentTimeMillis);

  uint32_t freq;
  memcpy(&freq, raw + offset, sizeof(freq));
  freq = ntohl(freq);
  memcpy(&(state->freq), &freq, sizeof(freq));
  offset += sizeof(freq);

  uint32_t bw;
  memcpy(&bw, raw + offset, sizeof(bw));
  bw = ntohl(bw);
  memcpy(&(state->bw), &bw, sizeof(bw));
  offset += sizeof(bw);

  memcpy(&(state->sf), raw + offset, sizeof(state->sf));
  offset += sizeof(state->sf);

  memcpy(&(state->cr), raw + offset, sizeof(state->cr));
  offset += sizeof(state->cr);

  memcpy(&(state->syncWord), raw + offset, sizeof(state->syncWord));
  offset += sizeof(state->syncWord);

  memcpy(&(state->power), raw + offset, sizeof(state->power));
  offset += sizeof(state->power);

  memcpy(&(state->preambleLength), raw + offset, sizeof(state->preambleLength));
  state->preambleLength = ntohs(state->preambleLength);
  offset += sizeof(state->preambleLength);

  memcpy(&(state->gain), raw + offset, sizeof(state->gain));
  offset += sizeof(state->gain);

  uint8_t ldo;
  memcpy(&ldo, raw + offset, sizeof(ldo));
  state->ldo = (ldo_type_t)ldo;
  offset += sizeof(ldo);

  memcpy(&state->useCrc, raw + offset, sizeof(state->useCrc));
  offset += sizeof(state->useCrc);

  memcpy(&state->useExplicitHeader, raw + offset, sizeof(state->useExplicitHeader));
  offset += sizeof(state->useExplicitHeader);

  memcpy(&state->length, raw + offset, sizeof(state->length));
  offset += sizeof(state->length);

  char buf[80];
  struct tm *ts;
  const char *format = "%Y-%m-%d %H:%M:%S";
  uint64_t timeSeconds = state->currentTimeMillis / 1000;
  ts = localtime((const time_t *)(&timeSeconds));
  strftime(buf, sizeof(buf), format, ts);
  log_i("current time: %s", buf);
  timeSeconds = state->startTimeMillis / 1000;
  ts = localtime((const time_t *)(&timeSeconds));
  strftime(buf, sizeof(buf), format, ts);
  log_i("start time:   %s", buf);
  timeSeconds = state->endTimeMillis / 1000;
  ts = localtime((const time_t *)(&timeSeconds));
  strftime(buf, sizeof(buf), format, ts);
  log_i("end time:     %s", buf);
  log_i("observation requested: %f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu,%hhu,%hhu,%hhu", state->freq, state->bw, state->sf, state->cr, state->syncWord, state->power, state->preambleLength, state->gain, state->ldo, state->useCrc, state->useExplicitHeader, state->length);
}

void LoRaShadowClient::sendData(lora_frame *frame) {
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
  length += sizeof(frame->frequency_error);
  length += sizeof(frame->rssi);
  length += sizeof(frame->snr);
  length += sizeof(frame->timestamp);
  length += sizeof(frame->data_length);
  length += frame->data_length;

  uint8_t *message = (uint8_t *)malloc(sizeof(uint8_t) * length);
  if (message == NULL) {
    return;
  }

  size_t offset = 0;

  int32_t frequency_error = htonl(frame->frequency_error);
  memcpy(message + offset, &frequency_error, sizeof(frequency_error));
  offset += sizeof(frequency_error);

  int16_t rssi = htons(frame->rssi);
  memcpy(message + offset, &rssi, sizeof(rssi));
  offset += sizeof(rssi);

  uint32_t snr;
  memcpy(&snr, &(frame->snr), sizeof(frame->snr));
  snr = htonl(snr);
  memcpy(message + offset, &snr, sizeof(snr));
  offset += sizeof(snr);

  uint64_t timestamp = htonll(frame->timestamp);
  memcpy(message + offset, &timestamp, sizeof(frame->timestamp));
  offset += sizeof(frame->timestamp);

  memcpy(message + offset, &frame->data_length, sizeof(frame->data_length));
  offset += sizeof(frame->data_length);

  memcpy(message + offset, frame->data, frame->data_length);
  offset += frame->data_length;

  req->writeValue(message, length, true);
  free(message);
}

uint64_t LoRaShadowClient::htonll(uint64_t x) {
#if __BIG_ENDIAN__
  return x;
#else
  return ((uint64_t)htonl((x)&0xFFFFFFFFLL) << 32) | htonl((x) >> 32);
#endif
}

uint64_t LoRaShadowClient::ntohll(uint64_t x) {
#if __BIG_ENDIAN__
  return x;
#else
  return ((uint64_t)ntohl((x)&0xFFFFFFFFLL) << 32) | ntohl((x) >> 32);
#endif
}
