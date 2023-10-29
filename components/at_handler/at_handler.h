#ifndef LORA_AT_AT_HANDLER_H
#define LORA_AT_AT_HANDLER_H

#include <esp_err.h>
#include <at_config.h>
#include <display.h>
#include <sx127x_util.h>
#include <at_util.h>
#include <ble_client.h>
#include <at_timer.h>

typedef struct {
  at_util_vector_t *frames;
  size_t buffer_length;
  char *output_buffer;
  lora_at_config_t *at_config;
  lora_at_display *display;
  sx127x *device;
  ble_client *bluetooth;
  at_timer_t *timer;

  char message[514];
  uint8_t message_hex[255];
  size_t message_hex_length;

  char syncword[18];
  uint8_t syncword_hex[16];
  size_t syncword_hex_length;

} at_handler_t;

esp_err_t at_handler_create(lora_at_config_t *at_config, lora_at_display *display, sx127x *device, ble_client *bluetooth, at_timer_t *timer, at_handler_t **handler);

void at_handler_process(char *input, size_t input_length, void (*callback)(char *, size_t, void *ctx), void *ctx, at_handler_t *handler);

esp_err_t at_handler_add_frame(lora_frame_t *frame, at_handler_t *handler);

void at_handler_destroy(at_handler_t *handler);

#endif //LORA_AT_AT_HANDLER_H
