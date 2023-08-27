#ifndef LORA_AT_AT_CONFIG_H
#define LORA_AT_AT_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

typedef struct {
  bool init_display;
  float min_freq;
  float max_freq;
  uint64_t bt_poll_period;
  char bt_address[6];
} lora_at_config_t;

esp_err_t lora_at_config_create(lora_at_config_t **config);

esp_err_t lora_at_config_set_display(bool init_display, lora_at_config_t *config);

void lora_at_config_destroy(lora_at_config_t *config);

#endif //LORA_AT_AT_CONFIG_H
