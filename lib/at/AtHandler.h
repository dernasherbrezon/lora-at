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
  void handlePull(Stream *in, Stream *out);
  size_t read_line(Stream *in);
  char buffer[BUFFER_LENGTH];
  LoRaModule *lora;
  Chips chips;
  bool receiving = false;
  std::vector<LoRaFrame *> receivedFrames;
};

#endif