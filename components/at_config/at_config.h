#ifndef LORA_AT_AT_CONFIG_H
#define LORA_AT_AT_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#define BT_ADDRESS_LENGTH 6

typedef struct {
  bool init_display;
  uint8_t *bt_address; // mac address in hex format
  uint64_t deep_sleep_period_micros;
  uint64_t inactivity_period_micros;
} lora_at_config_t;

esp_err_t lora_at_config_create(lora_at_config_t **config);

esp_err_t lora_at_config_set_display(bool init_display, lora_at_config_t *config);

esp_err_t lora_at_config_set_bt_address(uint8_t *bt_address, size_t bt_address_len, lora_at_config_t *config);

esp_err_t lora_at_config_set_dsconfig(uint64_t inactivity_period_micros, uint64_t deep_sleep_period_micros, lora_at_config_t *config);

void lora_at_config_destroy(lora_at_config_t *config);

#endif //LORA_AT_AT_CONFIG_H


