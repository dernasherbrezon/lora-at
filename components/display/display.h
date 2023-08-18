#ifndef LORA_AT_DISPLAY_H
#define LORA_AT_DISPLAY_H

#include <ssd1306.h>
#include <esp_err.h>

typedef struct {
  ssd1306_handle_t ssd1306_dev;
} lora_at_display_t;

esp_err_t lora_at_display_create(lora_at_display_t **display);

esp_err_t lora_at_display_set_status(const char *status, lora_at_display_t *display);

void lora_at_display_destroy(lora_at_display_t *display);

#endif //LORA_AT_DISPLAY_H
