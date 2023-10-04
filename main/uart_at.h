#ifndef LORA_AT_UART_AT_H
#define LORA_AT_UART_AT_H

#include <esp_err.h>
#include "at_handler.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

typedef struct {
  int uart_port_num;
  char *buffer;
  QueueHandle_t uart_queue;
  at_handler_t *handler;
} uart_at_handler_t;

esp_err_t uart_at_handler_create(at_handler_t *at_handler, uart_at_handler_t **result);

void uart_at_handler_process(uart_at_handler_t *handler);

void uart_at_handler_destroy(uart_at_handler_t *handler);

void uart_at_handler_send(char *message, void *handler);

#endif //LORA_AT_UART_AT_H
