#include "Chip.h"

Chip::Chip(const char *name, ChipType type) {
  this->name = name;
  this->type = type;
  switch (type) {
    // taken from sx1278 spec
    case TYPE_SX1278:
    case TYPE_SX1276_433:
      minLoraFrequency = 137.0;
      maxLoraFrequency = 525.0;
      loraSupported = true;

      minFskFrequency = 137.0;
      maxFskFrequency = 525.0;
      fskSupported = true;
      break;
    // taken from the min/max band plan
    // https://www.thethingsnetwork.org/docs/lorawan/regional-parameters/
    case TYPE_SX1276_868:
      minLoraFrequency = 863.0;
      maxLoraFrequency = 928.0;
      loraSupported = true;

      minFskFrequency = 863.0;
      maxFskFrequency = 928.0;
      fskSupported = true;
      break;
    // taken from sx1278 spec
    case TYPE_SX1276:
      minLoraFrequency = 137.0;
      maxLoraFrequency = 1020.0;
      loraSupported = true;

      minFskFrequency = 137.0;
      maxFskFrequency = 1020.0;
      fskSupported = true;
      break;
    default:
      break;
  }
}

const char *Chip::getName() {
  return this->name;
}
ChipType Chip::getType() {
  return this->type;
}