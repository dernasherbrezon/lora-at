#include "AtHandler.h"

#include <string.h>

// reading states
#define READING_CHARS 2
#define TIMEOUT 5

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0"
#endif

AtHandler::AtHandler(LoRaModule *lora) {
}

void AtHandler::handle(Stream *in, Stream *out) {
  size_t length = read_line(in);
  if (length == 0) {
    return;
  }
  if (strcmp("AT", this->buffer) == 0) {
    out->print("OK\r\n");
    return;
  }
  if (strcmp("AT+GMR", this->buffer) == 0) {
    out->print(FIRMWARE_VERSION);
    out->print("\r\n");
    return;
  }
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