#ifndef LORA_AT_AT_HANDLER_H
#define LORA_AT_AT_HANDLER_H

#include <esp_err.h>

typedef struct {
  size_t buffer_length;
  char *buffer;
  char *output_buffer;
  int uart_port_num;
} at_handler_t;

esp_err_t at_handler_create(size_t buffer_length, int uart_port_num, at_handler_t **handler);

void at_handler_process(at_handler_t *handler);

void at_handler_destroy(at_handler_t *handler);

#endif //LORA_AT_AT_HANDLER_H
