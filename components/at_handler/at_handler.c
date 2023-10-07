#include "at_handler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
#include <errno.h>
#include "sdkconfig.h"
#include <at_util.h>

#ifndef CONFIG_AT_UART_BUFFER_LENGTH
#define CONFIG_AT_UART_BUFFER_LENGTH 1024
#endif

#define ERROR_CHECK(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      at_handler_send_data(handler, callback, ctx, "%s: %s\r\nERROR\r\n", y, esp_err_to_name(__err_rc)); \
      return;        \
    }                         \
  } while (0)

esp_err_t at_handler_create(lora_at_config_t *at_config, lora_at_display *display, sx127x *device, ble_client *bluetooth, at_handler_t **handler) {
  at_handler_t *result = malloc(sizeof(at_handler_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->buffer_length = CONFIG_AT_UART_BUFFER_LENGTH;
  result->at_config = at_config;
  result->display = display;
  result->device = device;
  result->bluetooth = bluetooth;
  result->output_buffer = malloc(sizeof(uint8_t) * (result->buffer_length + 1)); // 1 is for \0
  if (result->output_buffer == NULL) {
    at_handler_destroy(result);
    return ESP_ERR_NO_MEM;
  }
  esp_err_t code = at_util_vector_create(&result->frames);
  if (code != ESP_OK) {
    at_handler_destroy(result);
    return code;
  }
  *handler = result;
  return ESP_OK;
}

void at_handler_send_data(at_handler_t *handler, void (*callback)(char *, void *ctx), void *ctx, const char *response, ...) {
  memset(handler->output_buffer, '\0', handler->buffer_length);
  va_list args;
  va_start (args, response);
  vsnprintf(handler->output_buffer, handler->buffer_length, response, args);
  va_end (args);
  callback(handler->output_buffer, ctx);
}

void at_handler_handle_pull(void (*callback)(char *, void *ctx), void *ctx, at_handler_t *handler) {
  for (size_t i = 0; i < at_util_vector_size(handler->frames); i++) {
    lora_frame_t *cur_frame = NULL;
    at_util_vector_get(i, (void *) &cur_frame, handler->frames);
    char *data = NULL;
    int code = at_util_hex2string(cur_frame->data, cur_frame->data_length, &data);
    if (code != 0) {
      callback("unable to convert to hex\r\n", ctx);
      lora_util_frame_destroy(cur_frame);
      continue;
    }
    at_handler_send_data(handler, callback, ctx, "%s,%d,%g,%d,%" PRIu64 "\r\n", data, cur_frame->rssi, cur_frame->snr, cur_frame->frequency_error, cur_frame->timestamp);
    free(data);
    lora_util_frame_destroy(cur_frame);
  }
  at_util_vector_clear(handler->frames);
  callback("OK\r\n", ctx);
}

esp_err_t at_handler_add_frame(lora_frame_t *frame, at_handler_t *handler) {
  return at_util_vector_add(frame, handler->frames);
}

void at_handler_process(char *input, size_t input_length, void (*callback)(char *, void *ctx), void *ctx, at_handler_t *handler) {
  if (strcmp("AT", input) == 0) {
    callback("OK\r\n", ctx);
    return;
  }
  if (strcmp("AT+DISPLAY?", input) == 0) {
    at_handler_send_data(handler, callback, ctx, "%d\r\nOK\r\n", (handler->at_config->init_display ? 1 : 0));
    return;
  }
  if (strcmp("AT+TIME?", input) == 0) {
    time_t timer = time(NULL);
    struct tm *tm_info = localtime(&timer);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    at_handler_send_data(handler, callback, ctx, "%s\r\nOK\r\n", buffer);
    return;
  }
  if (strcmp("AT+MINFREQ?", input) == 0) {
    at_handler_send_data(handler, callback, ctx, "%" PRIu64 "\r\nOK\r\n", lora_util_get_min_frequency());
    return;
  }
  if (strcmp("AT+MAXFREQ?", input) == 0) {
    at_handler_send_data(handler, callback, ctx, "%" PRIu64 "\r\nOK\r\n", lora_util_get_max_frequency());
    return;
  }
  if (strcmp("AT+STOPRX", input) == 0) {
    sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, handler->device);
    at_handler_handle_pull(callback, ctx, handler);
    lora_at_display_set_status("IDLE", handler->display);
    return;
  }
  if (strcmp("AT+PULL", input) == 0) {
    at_handler_handle_pull(callback, ctx, handler);
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
    callback("OK\r\n", ctx);
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
      at_handler_send_data(handler, callback, ctx, "%s\r\nERROR\r\n", strerror(errno));
    } else {
      callback("OK\r\n", ctx);
    }
    return;
  }
  rx_request_t state;
  matched = sscanf(input, "AT+LORARX=%" PRIu64 ",%" PRIu32 ",%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%d,%hhu,%hhu,%hhu", &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &state.ldo, &state.useCrc, &state.useExplicitHeader, &state.length);
  if (matched == 12) {
    ERROR_CHECK("unable to rx", lora_util_start_rx(&state, handler->device));
    callback("OK\r\n", ctx);
    lora_at_display_set_status("RX", handler->display);
    return;
  }

  char message[514];
  memset(message, '\0', sizeof(message));
  matched = sscanf(input, "AT+LORATX=%[^,],%" PRIu64 ",%" PRIu32 ",%hhu,%hhu,%hhu,%hhd,%hu,%hhu,%d,%hhu,%hhu,%hhu", message, &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.power, &state.preambleLength, &state.gain, &state.ldo, &state.useCrc, &state.useExplicitHeader,
                   &state.length);
  if (matched == 13) {
    uint8_t *binaryData = NULL;
    size_t binaryDataLength = 0;
    ERROR_CHECK("unable to convert HEX to byte array", at_util_string2hex(message, &binaryData, &binaryDataLength));
    lora_at_display_set_status("TX", handler->display);
    esp_err_t code = lora_util_start_tx(binaryData, binaryDataLength, &state, handler->device);
    if (code != ESP_OK) {
      lora_at_display_set_status("IDLE", handler->display);
      at_handler_send_data(handler, callback, ctx, "unable to tx: %s\r\nERROR\r\n", esp_err_to_name(code));
      return;
    }
    // will be sent from tx callback when message was actually sent
    // this will allow client applications to send next message
    // only when the previous was sent
//    callback("OK\r\n", ctx);
  }
  memset(message, '\0', sizeof(message));
  matched = sscanf(input, "AT+BLUETOOTH=%[^,]", message);
  if (matched == 1) {
    if (strlen(message) != 17) {
      at_handler_send_data(handler, callback, ctx, "invalid bluetooth address. expected: 00:00:00:00:00:00\r\nERROR\r\n");
      return;
    }
    uint8_t val[6];
    ERROR_CHECK("unable to convert address to hex", at_util_string2hex_allocated(message, val));
    ERROR_CHECK("unable to connect to bluetooth device", ble_client_connect(val, handler->bluetooth));
    ERROR_CHECK("unable to save config", lora_at_config_set_bt_address(message, handler->at_config));
    callback("OK\r\n", ctx);
    return;
  }
}

void at_handler_destroy(at_handler_t *handler) {
  if (handler == NULL) {
    return;
  }
  if (handler->output_buffer != NULL) {
    free(handler->output_buffer);
  }
  if (handler->frames != NULL) {
    at_util_vector_destroy(handler->frames);
  }
  free(handler);
}