#include "AtHandler.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <Util.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// reading states
#define READING_CHARS 2
#define TIMEOUT 5

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0"
#endif

AtHandler::AtHandler(LoRaModule *lora, Display *display, LoRaShadowClient *client, DeepSleepHandler *dsHandler) {
  this->lora = lora;
  this->display = display;
  this->client = client;
  this->dsHandler = dsHandler;
  this->loadConfig();
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

  if (strcmp("AT+CHIPS?", this->buffer) == 0) {
    this->handleQueryChips(out);
    return true;
  }

  if (strcmp("AT+CHIP?", this->buffer) == 0) {
    this->handleQueryChip(out);
    return true;
  }

  if (strcmp("AT+TIME?", this->buffer) == 0) {
    this->handleQueryTime(out);
    return true;
  }

  size_t chip_index = 0;
  int matched = sscanf(this->buffer, "AT+CHIP=%zu", &chip_index);
  if (matched == 1) {
    this->handleSetChip(chip_index, out);
    return true;
  }

  int enabled;
  matched = sscanf(this->buffer, "AT+DISPLAY=%d", &enabled);
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

  ObservationRequest state;
  uint8_t ldro;
  matched = sscanf(this->buffer, "AT+LORARX=%f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu", &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &ldro);
  if (matched == 9) {
    state.ldro = (LdroType)ldro;
    this->handleLoraRx(state, out);
    return true;
  }

  char message[512];
  matched = sscanf(this->buffer, "AT+LORATX=%[^,],%f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu", message, &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &ldro);
  if (matched == 10) {
    state.ldro = (LdroType)ldro;
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
    LoRaFrame *curFrame = this->receivedFrames[i];
    char *data = NULL;
    int code = convertHexToString(curFrame->getData(), curFrame->getDataLength(), &data);
    if (code != 0) {
      out->print("unable to convert hex\r\n");
      out->print("ERROR\r\n");
      return;
    }
    out->printf("%s,%g,%g,%g,%ld\r\n", data, curFrame->getRssi(), curFrame->getSnr(), curFrame->getFrequencyError(), curFrame->getTimestamp());
    free(data);
    delete curFrame;
  }
  this->receivedFrames.clear();
  out->print("OK\r\n");
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

void AtHandler::addFrame(LoRaFrame *frame) {
  this->receivedFrames.push_back(frame);
}

void AtHandler::handleQueryChip(Stream *out) {
  if (this->config_chip != NULL) {
    out->printf("%s\r\n", this->config_chip->getName());
    if (this->config_chip->loraSupported) {
      out->printf("LORA,%g,%g\r\n", this->config_chip->minLoraFrequency, this->config_chip->maxLoraFrequency);
    }
    if (this->config_chip->fskSupported) {
      out->printf("FSK,%g,%g\r\n", this->config_chip->minFskFrequency, this->config_chip->maxFskFrequency);
    }
  }
  out->print("OK\r\n");
}

void AtHandler::loadConfig() {
  if (!preferences.begin("lora-at", true)) {
    return;
  }
  if (!preferences.getBool("initialized")) {
    preferences.end();
    return;
  }
  size_t chip_index = preferences.getUChar("chip_index");
  preferences.end();
  // just invalid chip
  if (chip_index >= this->chips.getAll().size()) {
    return;
  }
  this->config_chip = this->chips.getAll().at(chip_index);

  int16_t code = this->lora->init(config_chip);
  if (code != ERR_NONE) {
    this->config_chip = NULL;
    return;
  }
}

void AtHandler::handleSetChip(size_t chip_index, Stream *out) {
  if (chip_index >= this->chips.getAll().size()) {
    out->printf("Unable to find chip index: %zu\r\n", chip_index);
    out->print("ERROR\r\n");
    return;
  }
  this->config_chip = this->chips.getAll().at(chip_index);

  int16_t code = this->lora->init(config_chip);
  if (code != ERR_NONE) {
    out->printf("Unable to initialize lora: %d\r\n", code);
    out->print("ERROR\r\n");
    return;
  }

  preferences.begin("lora-at", false);
  preferences.putBool("initialized", true);
  preferences.putUChar("chip_index", (uint8_t)chip_index);
  preferences.end();

  out->print("OK\r\n");
}

void AtHandler::handleStopRx(Stream *in, Stream *out) {
  if (!this->receiving) {
    out->print("OK\r\n");
    return;
  }
  this->receiving = false;
  this->lora->stopRx();
  this->handlePull(in, out);
}

void AtHandler::handleQueryChips(Stream *out) {
  for (size_t i = 0; i < this->chips.getAll().size(); i++) {
    out->printf("%zu,%s\r\n", i, this->chips.getAll().at(i)->getName());
  }
  out->print("OK\r\n");
}

void AtHandler::handleLoraRx(ObservationRequest state, Stream *out) {
  if (this->receiving) {
    out->printf("already receiving\r\n");
    out->print("ERROR\r\n");
    return;
  }
  int16_t code = this->lora->startLoraRx(&state);
  if (code == 0) {
    out->print("OK\r\n");
    this->receiving = true;
  } else {
    out->printf("Unable to start lora: %d\r\n", code);
    out->print("ERROR\r\n");
  }
}

void AtHandler::handleLoraTx(char *message, ObservationRequest state, Stream *out) {
  if (this->lora->isReceivingData()) {
    out->print("cannot transmit during receive\r\n");
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
  code = lora->loraTx(binaryData, binaryDataLength, &state);
  free(binaryData);
  if (code != 0) {
    out->printf("unable to send data: %d\r\n", code);
    out->print("ERROR\r\n");
    return;
  }
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
  if (!dsHandler->init(deepSleepPeriod, inactivityTimeout)) {
    out->printf("unable to configure deep sleep\r\n");
    out->print("ERROR\r\n");
    return;
  }

  this->display->setEnabled(false);

  out->print(BLEDevice::getAddress().toString().c_str());
  out->print("\r\n");
  out->print("OK\r\n");
}
