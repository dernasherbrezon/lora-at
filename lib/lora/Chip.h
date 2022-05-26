#ifndef Chip_h
#define Chip_h

enum ChipType {
  TYPE_SX1278 = 0,
  // SX1276 supports 137-1020, but boards configure them in either 433 or 868
  // while board 433 can send and receive 868 (and vice versa), performance differs significantly
  // thus 2 freq bands for the same chip
  TYPE_SX1276_433 = 1,
  TYPE_SX1276_868 = 2,
  TYPE_SX1276 = 3
};

class Chip {
 public:
  Chip(const char *name, ChipType type);
  const char *getName();
  ChipType getType();

  bool loraSupported = false;
  float minLoraFrequency = 0.0;
  float maxLoraFrequency = 0.0;

  bool fskSupported = false;
  float minFskFrequency = 0.0;
  float maxFskFrequency = 0.0;

 private:
  const char *name;
  ChipType type;
};

#endif