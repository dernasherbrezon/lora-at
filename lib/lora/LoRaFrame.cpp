#include "LoRaFrame.h"

#include <stdlib.h>

LoRaFrame::LoRaFrame() {
  // do nothing
}
LoRaFrame::~LoRaFrame() {
  if (this->data != NULL) {
    free(this->data);
  }
}
uint8_t *LoRaFrame::getData() {
  return this->data;
}
void LoRaFrame::setData(uint8_t *data) {
  this->data = data;
}
size_t LoRaFrame::getDataLength() {
  return this->dataLength;
}
void LoRaFrame::setDataLength(size_t dataLength) {
  this->dataLength = dataLength;
}
float LoRaFrame::getRssi() {
  return this->rssi;
}
void LoRaFrame::setRssi(float rssi) {
  this->rssi = rssi;
}
float LoRaFrame::getSnr() {
  return this->snr;
}
void LoRaFrame::setSnr(float snr) {
  this->snr = snr;
}
float LoRaFrame::getFrequencyError() {
  return this->frequencyError;
}
void LoRaFrame::setFrequencyError(float error) {
  this->frequencyError = error;
}
long LoRaFrame::getTimestamp() {
  return this->timestamp;
}
void LoRaFrame::setTimestamp(long timestamp) {
  this->timestamp = timestamp;
}