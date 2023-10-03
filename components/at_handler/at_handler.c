#include "at_handler.h"
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
#include <errno.h>
#include "sdkconfig.h"

#ifndef CONFIG_AT_UART_BUFFER_LENGTH
#define CONFIG_AT_UART_BUFFER_LENGTH 1024
#endif

#define ERROR_CHECK(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      at_handler_send_data(handler, output, output_length, "%s: %s\r\nFAIL\r\n", y, esp_err_to_name(__err_rc)); \
      return;        \
    }                         \
  } while (0)

esp_err_t at_handler_create(lora_at_config_t *at_config, lora_at_display *display, sx127x *device, at_handler_t **handler) {
  at_handler_t *result = malloc(sizeof(at_handler_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->buffer_length = CONFIG_AT_UART_BUFFER_LENGTH;
  result->at_config = at_config;
  result->display = display;
  result->device = device;
  result->output_buffer = malloc(sizeof(uint8_t) * (result->buffer_length + 1)); // 1 is for \0
  if (result->output_buffer == NULL) {
    at_handler_destroy(result);
    return ESP_ERR_NO_MEM;
  }
  *handler = result;
  return ESP_OK;
}

void at_handler_send_data(at_handler_t *handler, char **output, size_t *output_length, const char *response, ...) {
  memset(handler->output_buffer, '\0', handler->buffer_length);
  va_list args;
  va_start (args, response);
  vsnprintf(handler->output_buffer, handler->buffer_length, response, args);
  va_end (args);
  *output = handler->output_buffer;
  *output_length = strlen(handler->output_buffer);
}

void at_handler_handle_pull(char **output, size_t *output_length, at_handler_t *handler) {
  for (size_t i = 0; i < this->receivedFrames.size(); i++) {
    lora_frame *curFrame = this->receivedFrames[i];
    char *data = NULL;
    int code = util_hex2string(curFrame->data, curFrame->data_length, &data);
    if (code != 0) {
      out->print("unable to convert hex\r\n");
      out->print("ERROR\r\n");
      return;
    }
    out->printf("%s,%d,%g,%d,%" PRIu64 "\r\n", data, curFrame->rssi, curFrame->snr, curFrame->frequency_error, curFrame->timestamp);
    free(data);
    lora_util_frame_destroy(curFrame);
  }
  this->receivedFrames.clear();
  out->print("OK\r\n");
}

void at_handler_process(char *input, size_t input_length, char **output, size_t *output_length, at_handler_t *handler) {
  if (strcmp("AT", input) == 0) {
    at_handler_send_data(handler, output, output_length, "OK\r\n");
    return;
  }
  if (strcmp("AT+DISPLAY?", input) == 0) {
    at_handler_send_data(handler, output, output_length, "%d\r\nOK\r\n", (handler->at_config->init_display ? 1 : 0));
    return;
  }
  if (strcmp("AT+TIME?", input) == 0) {
    time_t timer = time(NULL);
    struct tm *tm_info = localtime(&timer);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    at_handler_send_data(handler, output, output_length, "%s\r\nOK\r\n", buffer);
    return;
  }
  if (strcmp("AT+MINFREQ?", input) == 0) {
    at_handler_send_data(handler, output, output_length, "%" PRIu64 "\r\nOK\r\n", handler->at_config->min_freq);
    return;
  }
  if (strcmp("AT+MAXFREQ?", input) == 0) {
    at_handler_send_data(handler, output, output_length, "%" PRIu64 "\r\nOK\r\n", handler->at_config->max_freq);
    return;
  }
  if (strcmp("AT+STOPRX", input) == 0) {
    sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, handler->device);
    at_handler_handle_pull(output, output_length, handler);
    return;
  }
  if (strcmp("AT+PULL", input) == 0) {
    at_handler_handle_pull(output, output_length, handler);
    return;
  }
  int enabled;
  int matched = sscanf(input, "AT+DISPLAY=%d", &enabled);
  if (matched == 1) {
    if (enabled) {
      ERROR_CHECK("unable to start", lora_at_display_start(handler->display));
    } else {
      ERROR_CHECK("unable to stop", lora_at_display_stop(handler->display));
    }
    ERROR_CHECK("unable to save config", lora_at_config_set_display(enabled, handler->at_config));
    at_handler_send_data(handler, output, output_length, "OK\r\n");
    return;
  }
  uint64_t freq;
  matched = sscanf(input, "AT+MINFREQ=%" PRIu64, &freq);
  if (matched == 1) {
    ERROR_CHECK("unable to save config", lora_at_config_set_min_freq(freq, handler->at_config));
    at_handler_send_data(handler, output, output_length, "OK\r\n");
    return;
  }
  matched = sscanf(input, "AT+MAXFREQ=%" PRIu64, &freq);
  if (matched == 1) {
    ERROR_CHECK("unable to save config", lora_at_config_set_max_freq(freq, handler->at_config));
    at_handler_send_data(handler, output, output_length, "OK\r\n");
    return;
  }
  uint64_t time;
  matched = sscanf(input, "AT+TIME=%" PRIu64, &time);
  if (matched == 1) {
    struct timeval now;
    now.tv_sec = (time_t) time;
    now.tv_usec = 0;
    int code = settimeofday(&now, NULL);
    if (code != 0) {
      at_handler_send_data(handler, output, output_length, "%s\r\nFAIL\r\n", strerror(errno));
    } else {
      at_handler_send_data(handler, output, output_length, "OK\r\n");
    }
    return;
  }
  rx_request_t state;
  matched = sscanf(input, "AT+LORARX=%" PRIu64 ",%f,%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%d,%hhu,%hhu,%hhu", &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &state.ldo, &state.useCrc, &state.useExplicitHeader, &state.length);
  if (matched == 12) {
    ERROR_CHECK("unable to start", lora_util_start_rx(&state, handler->device));
    at_handler_send_data(handler, output, output_length, "OK\r\n");
    return;
  }
  //FIXME add more commands here
}

void at_handler_destroy(at_handler_t *handler) {
  if (handler == NULL) {
    return;
  }
  if (handler->output_buffer != NULL) {
    free(handler->output_buffer);
  }
  free(handler);
}