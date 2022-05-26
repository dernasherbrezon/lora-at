#ifndef AtHandler_h
#define AtHandler_h

#include <Stream.h>
#include <LoRaModule.h>

#define BUFFER_LENGTH 257

class AtHandler {
    public:
        AtHandler(LoRaModule *lora);
        void handle(Stream *in, Stream *out);
    private:
        size_t read_line(Stream *in);
        char buffer[BUFFER_LENGTH];

};

#endif