#include "at_handler.h"
#include <driver/uart.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <time.h>
#include <esp_log.h>
#include "sdkconfig.h"

#ifndef CONFIG_AT_UART_PORT_NUM
#define CONFIG_AT_UART_PORT_NUM UART_NUM_0
#endif

#ifndef CONFIG_AT_UART_BAUD_RATE
#define CONFIG_AT_UART_BAUD_RATE 115200
#endif

#ifndef CONFIG_AT_UART_BUFFER_LENGTH
#define CONFIG_AT_UART_BUFFER_LENGTH 1024
#endif

#define ERROR_CHECK(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      at_handler_send_data(handler, "%s: %s\r\n", y, esp_err_to_name(__err_rc)); \
      at_handler_send_data(handler, "FAIL\r\n"); \
      return;        \
    }                         \
  } while (0)

#define ERROR_CHECK_ON_CREATE(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {                \
      at_handler_destroy(result);                                  \
      return __err_rc;        \
    }                         \
  } while (0)

esp_err_t at_handler_create(lora_at_config_t *at_config, lora_at_display *display, at_handler_t **handler) {
  at_handler_t *result = malloc(sizeof(at_handler_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->buffer_length = CONFIG_AT_UART_BUFFER_LENGTH;
  result->uart_port_num = CONFIG_AT_UART_PORT_NUM;
  result->at_config = at_config;
  result->display = display;
  result->buffer = malloc(sizeof(uint8_t) * (result->buffer_length + 1)); // 1 is for \0
  memset(result->buffer, 0, (result->buffer_length + 1));
  if (result->buffer == NULL) {
    at_handler_destroy(result);
    return ESP_ERR_NO_MEM;
  }
  result->output_buffer = malloc(sizeof(uint8_t) * (result->buffer_length + 1)); // 1 is for \0
  if (result->output_buffer == NULL) {
    at_handler_destroy(result);
    return ESP_ERR_NO_MEM;
  }
  uart_config_t uart_config = {
      .baud_rate = CONFIG_AT_UART_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  ERROR_CHECK_ON_CREATE(uart_driver_install(result->uart_port_num, result->buffer_length * 2, result->buffer_length * 2, 20, &result->uart_queue, 0));
  ERROR_CHECK_ON_CREATE(uart_param_config(result->uart_port_num, &uart_config));
  ERROR_CHECK_ON_CREATE(uart_set_pin(result->uart_port_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ERROR_CHECK_ON_CREATE(uart_enable_pattern_det_baud_intr(result->uart_port_num, '\n', 1, 9, 0, 0));
  ERROR_CHECK_ON_CREATE(uart_pattern_queue_reset(result->uart_port_num, 20));
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
  uart_event_t event;
  size_t current_index = 0;
  size_t buffered_size;
  while (1) {
    if (xQueueReceive(handler->uart_queue, (void *) &event, (TickType_t) portMAX_DELAY)) {
      switch (event.type) {
        //Event of UART receving data
        /*We'd better handler data event fast, there would be much more data events than
        other types of events. If we take too much time on data event, the queue might
        be full.*/
        case UART_DATA:
          uart_read_bytes(handler->uart_port_num, handler->buffer + current_index, event.size, portMAX_DELAY);
          current_index += event.size;
          break;
        case UART_PATTERN_DET:
          uart_get_buffered_data_len(handler->uart_port_num, &buffered_size);
          int pos = uart_pattern_pop_pos(handler->uart_port_num);
          if (pos == -1) {
            // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
            // record the position. We should set a larger queue size.
            // As an example, we directly flush the rx buffer here.
            uart_flush_input(handler->uart_port_num);
            current_index = 0;
          } else {
            // 1 is for pattern  - '\n'
            uart_read_bytes(handler->uart_port_num, handler->buffer + current_index, pos + 1, 100 / portTICK_PERIOD_MS);
            current_index += pos + 1;
          }
          break;
          //Event of HW FIFO overflow detected
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
          // If fifo overflow happened, you should consider adding flow control for your application.
          // The ISR has already reset the rx FIFO,
          // As an example, we directly flush the rx buffer here in order to read more data.
          uart_flush_input(handler->uart_port_num);
          xQueueReset(handler->uart_queue);
          current_index = 0;
          break;
        default:
          break;
      }
      if (current_index == 0) {
        continue;
      }
      bool found = false;
      // support for \r, \r\n or \n terminators
      if (handler->buffer[current_index - 1] == '\n') {
        handler->buffer[current_index - 1] = '\0';
        current_index--;
        found = true;
      }
      if (current_index > 0 && handler->buffer[current_index - 1] == '\r') {
        handler->buffer[current_index - 1] = '\0';
        current_index--;
        found = true;
      }
      if (found) {
        at_handler_process_message(handler);
        current_index = 0;
      }
    }
  }
}

void at_handler_destroy(at_handler_t *handler) {
  if (handler == NULL) {
    return;
  }
  uart_driver_delete(handler->uart_port_num);
  if (handler->buffer != NULL) {
    free(handler->buffer);
  }
  if (handler->output_buffer != NULL) {
    free(handler->output_buffer);
  }
  free(handler);
}