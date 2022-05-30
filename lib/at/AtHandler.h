#ifndef AtHandler_h
#define AtHandler_h

#include <Chips.h>
#include <LoRaModule.h>
#include <Display.h>
#include <Stream.h>
#include <Preferences.h>

#define BUFFER_LENGTH 1024

class AtHandler {
 public:
  AtHandler(LoRaModule *lora, Display *display);
  void handle(Stream *in, Stream *out);

 private:
  void handleLoraFrames();
  void handlePull(Stream *in, Stream *out);
  void handleSetChip(size_t chip_index, Stream *out);
  void handleQueryChip(Stream *out);
  void handleQueryChips(Stream *out);
  void handleStopRx(Stream *in, Stream *out);
  void handleLoraRx(LoraState state, Stream *out);
  void handleLoraTx(char *message, LoraState state, Stream *out);
  void handleQueryDisplay(Stream *out);
  void handleSetDisplay(bool enabled, Stream *out);
  void loadConfig();
  size_t read_line(Stream *in);
  char buffer[BUFFER_LENGTH];
  LoRaModule *lora;
  Display *display;
  Chips chips;
  bool receiving = false;
  std::vector<LoRaFrame *> receivedFrames;
  Preferences preferences;

  uint8_t config_version = 1;
  Chip *config_chip = NULL;
};

#endif