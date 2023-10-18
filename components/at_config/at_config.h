#ifndef LORA_AT_AT_CONFIG_H
#define LORA_AT_AT_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

typedef struct {
  bool init_display;
  char *bt_address; //00:00:00:00:00:00\0
  uint64_t deep_sleep_period_micros;
  uint64_t inactivity_period_micros;
} lora_at_config_t;

esp_err_t lora_at_config_create(lora_at_config_t **config);

esp_err_t lora_at_config_set_display(bool init_display, lora_at_config_t *config);

esp_err_t lora_at_config_set_bt_address(char *bt_address, lora_at_config_t *config);

void lora_at_config_destroy(lora_at_config_t *config);

#endif //LORA_AT_AT_CONFIG_H


