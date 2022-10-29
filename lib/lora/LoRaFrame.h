#ifndef LoRaFrame_h
#define LoRaFrame_h

#include <stddef.h>
#include <stdint.h>

struct LoRaFrame_t {
  float frequencyError;
  float rssi;
  float snr;
  uint64_t timestamp;
  uint32_t dataLength;
  uint8_t *data;
};

typedef LoRaFrame_t LoRaFrame;

void LoRaFrame_destroy(LoRaFrame *frame);

#endif