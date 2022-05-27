#include "AtHandler.h"

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
}

void AtHandler::handle(Stream *in, Stream *out) {
  size_t length = read_line(in);
  if (length == 0) {
    if (!this->receiving) {
      return;
    }
    LoRaFrame *frame = this->lora->loop();
    if (frame == NULL) {
      return;
    }
    this->receivedFrames.push_back(frame);
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
    if (!this->receiving) {
      out->print("OK\r\n");
      return;
    }
    this->receiving = false;
    this->lora->stopRx();
    this->handlePull(in, out);
    return;
  }

  if (strcmp("AT+PULL", this->buffer) == 0) {
    this->handlePull(in, out);
    return;
  }

  if (strcmp("AT+CHIPS?", this->buffer) == 0) {
    for (size_t i = 0; i < this->chips.getAll().size(); i++) {
      out->println(this->chips.getAll().at(i)->getName());
    }
    out->print("OK\r\n");
    return;
  }

  char chipName[255];
  memset(chipName, '\0', sizeof(chipName));
  int matched = sscanf(this->buffer, "AT+CHIP=%s", chipName);
  if (matched == 1) {
    Chip *found = NULL;
    for (size_t i = 0; i < this->chips.getAll().size(); i++) {
      if (strncmp(chipName, this->chips.getAll().at(i)->getName(), 255) == 0) {
        found = this->chips.getAll().at(i);
        break;
      }
    }
    if (found == NULL) {
      out->printf("Unable to find chip name: %s\r\n", chipName);
      out->print("ERROR\r\n");
      return;
    }

    int16_t code = this->lora->init(found);
    if (code != ERR_NONE) {
      out->printf("Unable to initialize lora: %d\r\n", code);
      out->print("ERROR\r\n");
      return;
    }

    //FIXME save chip into persistent memory
    return;
  }

  LoraState state;
  uint8_t ldro;
  matched = sscanf(this->buffer, "AT+LORARX=%f,%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%hhu", &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &ldro);
  if (matched == 9) {
    state.ldro = (LdroType)ldro;
    int16_t code = this->lora->startLoraRx(&state);
    if (code == 0) {
      out->print("OK\r\n");
      this->receiving = true;
    } else {
      out->printf("Unable to start lora: %d\r\n", code);
      out->print("ERROR\r\n");
    }
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
    out->printf("%s,%f,%f,%f,%ld\r\n", data, curFrame->getRssi(), curFrame->getSnr(), curFrame->getFrequencyError(), curFrame->getTimestamp());
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