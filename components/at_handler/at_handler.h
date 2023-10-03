#ifndef LORA_AT_AT_HANDLER_H
#define LORA_AT_AT_HANDLER_H

#include <esp_err.h>
#include <at_config.h>
#include <display.h>
#include <sx127x_util.h>

typedef struct {
  size_t buffer_length;
  char *output_buffer;
  lora_at_config_t *at_config;
  lora_at_display *display;
  sx127x *device;
} at_handler_t;

esp_err_t at_handler_create(lora_at_config_t *at_config, lora_at_display *display, sx127x *device, at_handler_t **handler);

void at_handler_process(char *input, size_t input_length, char **output, size_t *output_length, at_handler_t *handler);

void at_handler_destroy(at_handler_t *handler);

#endif //LORA_AT_AT_HANDLER_H
