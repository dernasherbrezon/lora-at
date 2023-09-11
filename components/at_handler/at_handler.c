#include "at_handler.h"
#include <driver/uart.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <time.h>

#define ERROR_CHECK(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      at_handler_send_data(handler, "%s: %d\r\n", y, __err_rc); \
      at_handler_send_data(handler, "FAIL\r\n"); \
      return;        \
    }                         \
  } while (0)

esp_err_t at_handler_create(size_t buffer_length, int uart_port_num, lora_at_config_t *at_config, lora_at_display *display, at_handler_t **handler) {
  at_handler_t *result = malloc(sizeof(at_handler_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->buffer_length = buffer_length;
  result->uart_port_num = uart_port_num;
  result->at_config = at_config;
  result->display = display;
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
  *handler = result;
  return ESP_OK;
}

void at_handler_send_data(at_handler_t *handler, const char *response, ...) {
  memset(handler->output_buffer, '\0', handler->buffer_length);
  va_list args;
  va_start (args, response);
  vsnprintf(handler->output_buffer, handler->buffer_length, response, args);
  va_end (args);
  uart_write_bytes(handler->uart_port_num, handler->output_buffer, strlen(handler->output_buffer));
}

void at_handler_process_message(at_handler_t *handler) {
  if (strcmp("AT", handler->buffer) == 0) {
    at_handler_send_data(handler, "OK\r\n");
    return;
  }
  if (strcmp("AT+DISPLAY?", handler->buffer) == 0) {
    at_handler_send_data(handler, "%d\r\n", (handler->at_config->init_display ? 1 : 0));
    at_handler_send_data(handler, "OK\r\n");
    return;
  }
  if (strcmp("AT+TIME?", handler->buffer) == 0) {
    time_t timer = time(NULL);
    struct tm *tm_info = localtime(&timer);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    at_handler_send_data(handler, "%s\r\n", buffer);
    at_handler_send_data(handler, "OK\r\n");
    return;
  }
  int enabled;
  int matched = sscanf(handler->buffer, "AT+DISPLAY=%d", &enabled);
  if (matched == 1) {
    if (enabled) {
      ERROR_CHECK("unable to start", lora_at_display_start(handler->display));
    } else {
      ERROR_CHECK("unable to stop", lora_at_display_stop(handler->display));
    }
    ERROR_CHECK("unable to save config", lora_at_config_set_display(enabled, handler->at_config));
    at_handler_send_data(handler, "OK\r\n");
    return;
  }
  //FIXME add more commands here
}

void at_handler_process(at_handler_t *handler) {
  size_t current_index = 0;
  while (1) {
    const int rxBytes = uart_read_bytes(handler->uart_port_num, handler->buffer + current_index, handler->buffer_length - current_index, pdMS_TO_TICKS(1000));
    if (rxBytes < 0) {
      break;
    }
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