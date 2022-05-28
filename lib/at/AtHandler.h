#ifndef AtHandler_h
#define AtHandler_h

#include <Chips.h>
#include <LoRaModule.h>
#include <Stream.h>

#define BUFFER_LENGTH 257

class AtHandler {
 public:
  AtHandler(LoRaModule *lora);
  void handle(Stream *in, Stream *out);

 private:
  void handleLoraFrames();
  void handlePull(Stream *in, Stream *out);
  void handleChip(size_t chip_index, Stream *out);
  void loadConfig();
  size_t read_line(Stream *in);
  char buffer[BUFFER_LENGTH];
  LoRaModule *lora;
  Chips chips;
  bool receiving = false;
  std::vector<LoRaFrame *> receivedFrames;

  uint8_t config_version = 1;
  Chip *config_chip = NULL;
};

#endif