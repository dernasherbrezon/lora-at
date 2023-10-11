#ifndef LORA_AT_DEEP_SLEEP_H
#define LORA_AT_DEEP_SLEEP_H

#include <stdint.h>

void deep_sleep_enter(uint64_t micros_to_wait);

void deep_sleep_rx_enter(uint64_t micros_to_wait);

#endif //LORA_AT_DEEP_SLEEP_H
