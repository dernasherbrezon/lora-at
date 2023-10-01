#ifndef LORA_AT_AT_HANDLER_H
#define LORA_AT_AT_HANDLER_H

#include <esp_err.h>
#include <at_config.h>
#include <display.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

typedef struct {
  size_t buffer_length;
  char *buffer;
  char *output_buffer;
  int uart_port_num;
  lora_at_config_t *at_config;
  lora_at_display *display;
  QueueHandle_t uart_queue;
} at_handler_t;

esp_err_t at_handler_create(lora_at_config_t *at_config, lora_at_display *display, at_handler_t **handler);

void at_handler_process(at_handler_t *handler);

void at_handler_destroy(at_handler_t *handler);

#endif //LORA_AT_AT_HANDLER_H
