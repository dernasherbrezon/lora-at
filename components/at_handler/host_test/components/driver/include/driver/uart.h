
#ifndef LORA_AT_UART_H
#define LORA_AT_UART_H

#include <esp_err.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>

typedef int uart_port_t;

int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size);

int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length, TickType_t ticks_to_wait);

#endif //LORA_AT_UART_H
