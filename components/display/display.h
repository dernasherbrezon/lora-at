#ifndef LORA_AT_DISPLAY_H
#define LORA_AT_DISPLAY_H

#include <esp_err.h>

typedef struct lora_at_display_t lora_at_display;

esp_err_t lora_at_display_create(lora_at_display **display);

esp_err_t lora_at_display_start(lora_at_display *display);

esp_err_t lora_at_display_stop(lora_at_display *display);

esp_err_t lora_at_display_set_status(const char *status, lora_at_display *display);

void lora_at_display_destroy(lora_at_display *display);

#endif //LORA_AT_DISPLAY_H
