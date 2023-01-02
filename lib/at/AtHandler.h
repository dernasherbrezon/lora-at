#ifndef AtHandler_h
#define AtHandler_h

#include <DeepSleepHandler.h>
#include <Display.h>
#include <LoRaShadowClient.h>
#include <Preferences.h>
#include <Stream.h>
#include <lora_util.h>

#define BUFFER_LENGTH 1024

class AtHandler {
 public:
  AtHandler(sx127x *device, Display *display, LoRaShadowClient *client, DeepSleepHandler *dsHandler);
  bool handle(Stream *in, Stream *out);
  void addFrame(lora_frame *frame);
  boolean isReceiving();
  void setTransmitting(boolean value);

 private:
  void handlePull(Stream *in, Stream *out);
  void handleStopRx(Stream *in, Stream *out);
  void handleLoraRx(rx_request state, Stream *out);
  void handleLoraTx(char *message, rx_request state, Stream *out);
  void handleQueryDisplay(Stream *out);
  void handleSetDisplay(bool enabled, Stream *out);
  void handleQueryTime(Stream *out);
  void handleSetTime(unsigned long time, Stream *out);
  void handleMinimumFrequency(Stream *out);
  void handleMaximumFrequency(Stream *out);
  void handleSetMinimumFrequency(float freq, Stream *out);
  void handleSetMaximumFrequency(float freq, Stream *out);
  void handleDeepSleepConfig(uint8_t *btaddress, size_t address_len, uint64_t deepSleepPeriod, uint64_t inactivityTimeout, Stream *out);
  size_t read_line(Stream *in);
  char buffer[BUFFER_LENGTH];
  sx127x *device;
  Display *display;
  LoRaShadowClient *client;
  DeepSleepHandler *dsHandler;
  bool receiving = false;
  bool transmitting = false;
  std::vector<lora_frame *> receivedFrames;
  Preferences preferences;
  uint8_t config_version = 1;
  float minimumFrequency = 137.0f;
  float maximumFrequency = 1020.0f;
};

#endif