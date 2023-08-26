#include "at_handler.h"
#include <driver/uart.h>
#include <string.h>

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      at_handler_destroy(result);                        \
      return __err_rc;        \
    }                         \
  } while (0)

esp_err_t at_handler_create(at_handler_t **handler) {
  at_handler_t *result = malloc(sizeof(at_handler_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->buffer_length = 1024;
  result->buffer = malloc(sizeof(uint8_t) * (result->buffer_length + 1)); // 1 is for \0
  if (result->buffer == NULL) {
    at_handler_destroy(result);
    return ESP_ERR_NO_MEM;
  }
  result->output_buffer = malloc(sizeof(uint8_t) * (result->buffer_length + 1)); // 1 is for \0
  if (result->output_buffer == NULL) {
    at_handler_destroy(result);
    return ESP_ERR_NO_MEM;
  }
  const uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  result->uart_port_num = UART_NUM_0;
  // We won't use a buffer for sending data.
  ERROR_CHECK(uart_driver_install(result->uart_port_num, result->buffer_length * 2, 0, 0, NULL, 0));
  ERROR_CHECK(uart_param_config(result->uart_port_num, &uart_config));
  //FIXME most likely pins were already configured
//  uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  *handler = result;
  return ESP_OK;
}

void at_handler_send_data(at_handler_t *handler, const char *response, ...) {
  memset(handler->output_buffer, '\0', handler->buffer_length);
  va_list args;
  va_start (args, response);
  snprintf(handler->output_buffer, handler->buffer_length, response, args);
  va_end (args);
  uart_write_bytes(handler->uart_port_num, response, strlen(response));
}

void at_handler_process_message(at_handler_t *handler) {
  if (strcmp("AT", handler->buffer) == 0) {
    at_handler_send_data(handler, "OK\r\n");
    return;
  }
  if (strcmp("AT+DISPLAY?", handler->buffer) == 0) {
    at_handler_send_data(handler, "%d\t\n", 123);
    at_handler_send_data(handler, "OK\r\n");
    return;
  }
  //FIXME add more commands here
}

void at_handler_process(at_handler_t *handler) {
  size_t current_index = 0;
  while (1) {
    const int rxBytes = uart_read_bytes(handler->uart_port_num, handler->buffer + current_index, handler->buffer_length - current_index, 1000 / portTICK_PERIOD_MS);
    size_t current_message_size = current_index + rxBytes;
    bool found = false;
    for (size_t i = current_index; i < current_message_size; i++) {
      if (handler->buffer[i] == '\n') {
        if (i > 0 && handler->buffer[i - 1] == '\r') {
          handler->buffer[i - 1] = '\0';
        } else {
          handler->buffer[i] = '\0';
        }
        found = true;
        break;
      }
    }
    if (found) {
      at_handler_process_message(handler);
      current_index = 0;
    } else {
      current_index = current_message_size;
    }
  }
}

void at_handler_destroy(at_handler_t *handler) {
  if (handler == NULL) {
    return;
  }
  if (handler->buffer != NULL) {
    free(handler->buffer);
  }
  if (handler->output_buffer != NULL) {
    free(handler->output_buffer);
  }
  free(handler);
}