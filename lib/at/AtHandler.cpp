#include "AtHandler.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <Util.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// reading states
#define READING_CHARS 2
#define TIMEOUT 5

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0"
#endif

AtHandler::AtHandler(sx127x *device, Display *display, LoRaShadowClient *client, DeepSleepHandler *dsHandler) {
  this->device = device;
  this->display = display;
  this->client = client;
  this->dsHandler = dsHandler;
}

bool AtHandler::handle(Stream *in, Stream *out) {
  size_t length = read_line(in);
  if (length == 0) {
    return false;
  }
  if (strcmp("AT", this->buffer) == 0) {
    out->print("OK\r\n");
    return true;
  }
  if (strcmp("AT+GMR", this->buffer) == 0) {
    out->print(FIRMWARE_VERSION);
    out->print("\r\n");
    out->print("OK\r\n");
    return true;
  }
  if (strcmp("AT+STOPRX", this->buffer) == 0) {
    this->handleStopRx(in, out);
    return true;
  }

  if (strcmp("AT+PULL", this->buffer) == 0) {
    this->handlePull(in, out);
    return true;
  }

  if (strcmp("AT+DISPLAY?", this->buffer) == 0) {
    this->handleQueryDisplay(out);
    return true;
  }

  if (strcmp("AT+TIME?", this->buffer) == 0) {
    this->handleQueryTime(out);
    return true;
  }

  int enabled;
  int matched = sscanf(this->buffer, "AT+DISPLAY=%d", &enabled);
  if (matched == 1) {
    this->handleSetDisplay(enabled, out);
    return true;
  }

  unsigned long time;
  matched = sscanf(this->buffer, "AT+TIME=%lu", &time);
  if (matched == 1) {
    this->handleSetTime(time, out);
    return true;
  }

  rx_request state;
  uint8_t ldo;
  matched = sscanf(this->buffer, "AT+LORARX=%f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu", &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &ldo);
  if (matched == 9) {
    state.ldo = (ldo_type_t)ldo;
    this->handleLoraRx(state, out);
    return true;
  }

  char message[512];
  matched = sscanf(this->buffer, "AT+LORATX=%[^,],%f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu", message, &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &ldo);
  if (matched == 10) {
    state.ldo = (ldo_type_t)ldo;
    this->handleLoraTx(message, state, out);
    return true;
  }

  uint8_t address[6];
  uint64_t deepSleepPeriod = 0L;
  uint64_t inactivityTimeout = 0L;
  matched = sscanf(this->buffer, "AT+DSCONFIG=%hhx:%hhx:%hhx:%hhx:%hhx:%hhx,%" SCNu64 ",%" SCNu64 "", &address[0], &address[1], &address[2], &address[3], &address[4], &address[5], &deepSleepPeriod, &inactivityTimeout);
  if (matched == 8) {
    this->handleDeepSleepConfig(address, sizeof(address), deepSleepPeriod, inactivityTimeout, out);
    return true;
  }

  if (strcmp("AT+DS", this->buffer) == 0) {
    this->dsHandler->enterDeepSleep(0);
    return true;
  }

  out->print("unknown command\r\n");
  out->print("ERROR\r\n");
  return true;
}

void AtHandler::handlePull(Stream *in, Stream *out) {
  for (size_t i = 0; i < this->receivedFrames.size(); i++) {
    lora_frame *curFrame = this->receivedFrames[i];
    char *data = NULL;
    int code = convertHexToString(curFrame->data, curFrame->data_length, &data);
    if (code != 0) {
      out->print("unable to convert hex\r\n");
      out->print("ERROR\r\n");
      return;
    }
    out->printf("%s,%d,%g,%d,%" PRIu64 "\r\n", data, curFrame->rssi, curFrame->snr, curFrame->frequency_error, curFrame->timestamp);
    free(data);
    lora_util_frame_destroy(curFrame);
  }
  this->receivedFrames.clear();
  out->print("OK\r\n");
}

boolean AtHandler::isReceiving() {
  return this->receiving;
}

void AtHandler::setTransmitting(boolean value) {
  this->transmitting = value;
}

size_t AtHandler::read_line(Stream *in) {
  // Check to see if anything is available in the serial receive buffer
  while (in->available() > 0) {
    static unsigned int message_pos = 0;
    // Read the next available byte in the serial receive buffer
    char inByte = in->read();
    if (inByte == '\r') {
      continue;
    }
    // Message coming in (check not terminating character) and guard for over message size
    if (inByte != '\n' && (message_pos < BUFFER_LENGTH - 1)) {
      // Add the incoming byte to our message
      this->buffer[message_pos] = inByte;
      message_pos++;
    } else {
      // Add null character to string
      this->buffer[message_pos] = '\0';
      size_t result = message_pos;
      message_pos = 0;
      return result;
    }
  }
  return 0;
}

void AtHandler::addFrame(lora_frame *frame) {
  this->receivedFrames.push_back(frame);
}

void AtHandler::handleStopRx(Stream *in, Stream *out) {
  if (!this->receiving) {
    out->print("OK\r\n");
    return;
  }
  this->receiving = false;
  sx127x_set_opmod(SX127x_MODE_SLEEP, device);
  this->handlePull(in, out);
}

void AtHandler::handleLoraRx(rx_request state, Stream *out) {
  if (this->receiving) {
    out->printf("already receiving\r\n");
    out->print("ERROR\r\n");
    return;
  }
  if (this->transmitting) {
    out->print("still transmitting previous message\r\n");
    out->print("ERROR\r\n");
    return;
  }
  esp_err_t code = lora_util_start_rx(&state, device);
  if (code == ESP_OK) {
    out->print("OK\r\n");
    this->receiving = true;
  } else {
    out->printf("Unable to start lora: %d\r\n", code);
    out->print("ERROR\r\n");
  }
}

void AtHandler::handleLoraTx(char *message, rx_request state, Stream *out) {
  if (this->receiving) {
    out->print("cannot transmit during receive\r\n");
    out->print("ERROR\r\n");
    return;
  }
  if (this->transmitting) {
    out->print("still transmitting previous message\r\n");
    out->print("ERROR\r\n");
    return;
  }
  uint8_t *binaryData = NULL;
  size_t binaryDataLength = 0;
  int code = convertStringToHex(message, &binaryData, &binaryDataLength);
  if (code != 0) {
    out->printf("unable to convert HEX to byte array: %d\r\n", code);
    out->print("ERROR\r\n");
    return;
  }
  code = lora_util_start_tx(binaryData, binaryDataLength, &state, this->device);
  free(binaryData);
  if (code != 0) {
    out->printf("unable to send data: %d\r\n", code);
    out->print("ERROR\r\n");
    return;
  }
  this->transmitting = true;
  out->print("OK\r\n");
}

void AtHandler::handleQueryDisplay(Stream *out) {
  out->printf("%d\r\n", this->display->isEnabled());
  out->print("OK\r\n");
}

void AtHandler::handleSetDisplay(bool enabled, Stream *out) {
  this->display->setEnabled(enabled);
  out->print("OK\r\n");
}

void AtHandler::handleQueryTime(Stream *out) {
  time_t timer;
  char buffer[26];
  struct tm *tm_info;

  timer = time(NULL);
  tm_info = localtime(&timer);

  strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

  out->printf("%s\r\n", buffer);
  out->print("OK\r\n");
}

void AtHandler::handleSetTime(unsigned long time, Stream *out) {
  struct timeval now;
  now.tv_sec = time;
  now.tv_usec = 0;
  int code = settimeofday(&now, NULL);
  if (code != 0) {
    out->printf("unable to set time: %d\r\n", code);
    out->print("ERROR\r\n");
  } else {
    out->print("OK\r\n");
  }
}

void AtHandler::handleDeepSleepConfig(uint8_t *address, size_t address_len, uint64_t deepSleepPeriod, uint64_t inactivityTimeout, Stream *out) {
  if (!client->init(address, address_len)) {
    out->printf("unable to connect to lora-shadow: %x:%x:%x:%x:%x:%x make sure process is started somewhere\r\n", address[0], address[1], address[2], address[3], address[4], address[5]);
    out->print("ERROR\r\n");
    return;
  }
  if (!dsHandler->init(deepSleepPeriod * 1000, inactivityTimeout * 1000)) {
    out->printf("unable to configure deep sleep\r\n");
    out->print("ERROR\r\n");
    return;
  }

  this->display->setEnabled(false);

  // FIXME
  //  out->printf("%s,%g,%g\r\n", BLEDevice::getAddress().toString().c_str(), this->config_chip->minLoraFrequency, this->config_chip->maxLoraFrequency);
  out->print("OK\r\n");
}
