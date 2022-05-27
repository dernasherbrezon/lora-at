#include "Chips.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRING_LEN 256

#ifndef ARDUINO_VARIANT
#define ARDUINO_VARIANT "native"
#endif

Chips::Chips() {
  // each board has limited set of semtech chips
  if ((strncmp("ttgo-lora32-v2", ARDUINO_VARIANT, 255) == 0) ||
      (strncmp("ttgo-lora32-v1", ARDUINO_VARIANT, 255) == 0) ||
      (strncmp("ttgo-lora32-v21", ARDUINO_VARIANT, 255) == 0)) {
    this->chips.push_back(new Chip("TTGO - 433/470Mhz", ChipType::TYPE_SX1276_433));
    this->chips.push_back(new Chip("TTGO - 868/915Mhz", ChipType::TYPE_SX1276_868));
  } else if((strncmp("heltec_wifi_lora_32", ARDUINO_VARIANT, 255) == 0) ||
            (strncmp("heltec_wifi_lora_32_V2", ARDUINO_VARIANT, 255) == 0)) {
    this->chips.push_back(new Chip("SX1278 - 433/470Mhz", ChipType::TYPE_SX1278));
    this->chips.push_back(new Chip("SX1276 - 868/915Mhz", ChipType::TYPE_SX1276_868));
  } else if(strncmp("ttgo-t-beam", ARDUINO_VARIANT, 255) == 0) {
    this->chips.push_back(new Chip("TTGO - 433/470Mhz", ChipType::TYPE_SX1278));
    this->chips.push_back(new Chip("TTGO - 868/915Mhz", ChipType::TYPE_SX1276_868));
  } else {
    this->chips.push_back(new Chip("SX1278 - 433/470Mhz", ChipType::TYPE_SX1278));
    this->chips.push_back(new Chip("SX1276 - 433/868/915Mhz", ChipType::TYPE_SX1276));
  }

  this->chip_names = (char *)malloc(sizeof(char) * STRING_LEN * this->chips.size());
  this->chip_indices = (char *)malloc(sizeof(char) * STRING_LEN * this->chips.size());

  for (size_t i = 0; i < this->chips.size(); i++) {
    size_t original_len = strnlen(this->chips[i]->getName(), STRING_LEN);
    strncpy(this->chip_names + i * STRING_LEN, this->chips[i]->getName(), original_len + 1);
    snprintf(this->chip_indices + i * STRING_LEN, STRING_LEN, "%d", i);
  }
}

char *Chips::getChipNames() {
  return this->chip_names;
}
char *Chips::getChipIndices() {
  return this->chip_indices;
}
std::vector<Chip *> Chips::getAll() {
  return this->chips;
}