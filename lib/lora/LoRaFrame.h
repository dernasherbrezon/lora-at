#ifndef LoRaFrame_h
#define LoRaFrame_h

#include <stddef.h>
#include <stdint.h>

class LoRaFrame {
 public:
  LoRaFrame();
  ~LoRaFrame();
  uint8_t *getData();
  void setData(uint8_t *data);
  size_t getDataLength();
  void setDataLength(size_t dataLength);
  float getRssi();
  void setRssi(float rssi);
  float getSnr();
  void setSnr(float snr);
  float getFrequencyError();
  void setFrequencyError(float error);
  long getTimestamp();
  void setTimestamp(long timestamp);

 private:
  uint8_t *data = NULL;
  size_t dataLength = 0;
  float rssi = 0.0F;
  float snr = 0.0F;
  float frequencyError = 0.0F;
  long timestamp = 0L;
};

#endif