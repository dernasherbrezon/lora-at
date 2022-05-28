#include "AtHandler.h"

#include <EEPROM.h>
#include <Util.h>
#include <string.h>

// reading states
#define READING_CHARS 2
#define TIMEOUT 5

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0"
#endif

AtHandler::AtHandler(LoRaModule *lora) {
  this->lora = lora;
  this->loadConfig();
}

void AtHandler::handle(Stream *in, Stream *out) {
  size_t length = read_line(in);
  if (length == 0) {
    this->handleLoraFrames();
    return;
  }
  if (strcmp("AT", this->buffer) == 0) {
    out->print("OK\r\n");
    return;
  }
  if (strcmp("AT+GMR", this->buffer) == 0) {
    out->print(FIRMWARE_VERSION);
    out->print("\r\n");
    out->print("OK\r\n");
    return;
  }
  if (strcmp("AT+STOPRX", this->buffer) == 0) {
    this->handleStopRx(in, out);
    return;
  }

  if (strcmp("AT+PULL", this->buffer) == 0) {
    this->handlePull(in, out);
    return;
  }

  if (strcmp("AT+CHIPS?", this->buffer) == 0) {
    this->handleQueryChips(out);
    return;
  }

  if (strcmp("AT+CHIP?", this->buffer) == 0) {
    this->handleQueryChip(out);
    return;
  }

  size_t chip_index = 0;
  int matched = sscanf(this->buffer, "AT+CHIP=%zu", &chip_index);
  if (matched == 1) {
    this->handleSetChip(chip_index, out);
    return;
  }

  LoraState state;
  uint8_t ldro;
  matched = sscanf(this->buffer, "AT+LORARX=%f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu", &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &ldro);
  if (matched == 9) {
    state.ldro = (LdroType)ldro;
    this->handleLoraRx(state, out);
    return;
  }

  char message[512];
  matched = sscanf(this->buffer, "AT+LORATX=%s,%f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu", message, &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &ldro);
  if (matched == 10) {
    state.ldro = (LdroType)ldro;
    this->handleLoraTx(message, state, out);
    return;
  }

  out->print("unknown command\r\n");
  out->print("ERROR\r\n");
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
  size_t result = 0;
  byte b;
  unsigned long start_time = millis();
  unsigned long timeout = 1000L;
  memset(this->buffer, '\0', BUFFER_LENGTH);
  int reading_state = READING_CHARS;
  while (result < BUFFER_LENGTH) {
    switch (reading_state) {
      case READING_CHARS:
        b = in->read();
        if (b != -1) {
          // A byte has been received
          if ((char)b == '\r') {
            return result;
          } else {
            this->buffer[result] = b;
            result++;
          }
        } else {
          reading_state = TIMEOUT;
        }
        break;
      case TIMEOUT:
        // Control if timeout is expired
        if ((millis() - start_time) > timeout) {
          // Timeout expired
          return 0;
        } else {
          // Continue reading chars
          reading_state = READING_CHARS;
        }
        break;
    }  // end switch reading_state
  }
  // buffer is full
  return 0;
}

void AtHandler::handleLoraFrames() {
  if (!this->receiving) {
    return;
  }
  LoRaFrame *frame = this->lora->loop();
  if (frame == NULL) {
    return;
  }
  this->receivedFrames.push_back(frame);
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

  if (!EEPROM.begin(sizeof(uint8_t) + sizeof(size_t))) {
    out->print("unable to open EEPROM for storing data\r\n");
    out->print("ERROR\r\n");
    return;
  }
  EEPROM.writeAll(0, this->config_version);
  EEPROM.writeAll(sizeof(this->config_version), chip_index);
  EEPROM.end();

  out->print("OK\r\n");
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
  if (!EEPROM.begin(sizeof(uint8_t) + sizeof(size_t))) {
    return;
  }
  if (EEPROM.readByte(0) != this->config_version) {
    EEPROM.end();
    return;
  }
  size_t chip_index = 0;
  chip_index = EEPROM.readAll(sizeof(uint8_t), chip_index);
  EEPROM.end();
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

void AtHandler::handleLoraRx(LoraState state, Stream *out) {
  int16_t code = this->lora->startLoraRx(&state);
  if (code == 0) {
    out->print("OK\r\n");
    this->receiving = true;
  } else {
    out->printf("Unable to start lora: %d\r\n", code);
    out->print("ERROR\r\n");
  }
}

void AtHandler::handleLoraTx(char *message, LoraState state, Stream *out) {
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