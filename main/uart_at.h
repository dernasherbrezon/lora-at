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
  at_timer_t *timer;
  uint64_t last_active_micros;
} uart_at_handler_t;

esp_err_t uart_at_handler_create(at_handler_t *at_handler, at_timer_t *timer, uart_at_handler_t **result);

void uart_at_handler_process(uart_at_handler_t *handler);

void uart_at_handler_destroy(uart_at_handler_t *handler);

esp_err_t uart_at_get_last_active(uint64_t *last_active_micros, uart_at_handler_t *handler);

void uart_at_handler_send(char *output, size_t output_length, void *handler);

#endif //LORA_AT_UART_AT_H
