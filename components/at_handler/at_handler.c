#include "at_handler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
#include <errno.h>
#include <sdkconfig.h>
#include <at_util.h>
#include <esp_mac.h>

#ifndef CONFIG_AT_UART_BUFFER_LENGTH
#define CONFIG_AT_UART_BUFFER_LENGTH 1024
#endif

#define ERROR_CHECK(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      at_handler_respond(handler, callback, ctx, "%s: %s\r\nERROR\r\n", y, esp_err_to_name(__err_rc)); \
      return;        \
    }                         \
  } while (0)

esp_err_t at_handler_create(lora_at_config_t *at_config, lora_at_display *display, sx127x_wrapper *device, ble_client *bluetooth, at_timer_t *timer, at_handler_t **handler) {
  at_handler_t *result = malloc(sizeof(at_handler_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->buffer_length = CONFIG_AT_UART_BUFFER_LENGTH;
  result->at_config = at_config;
  result->display = display;
  result->device = device;
  result->bluetooth = bluetooth;
  result->timer = timer;
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

void at_handler_respond(at_handler_t *handler, void (*callback)(char *, size_t, void *ctx), void *ctx, const char *response, ...) {
  memset(handler->output_buffer, 0, handler->buffer_length);
  va_list args;
  va_start (args, response);
  vsnprintf(handler->output_buffer, handler->buffer_length, response, args);
  va_end (args);
  callback(handler->output_buffer, strlen(handler->output_buffer), ctx);
}

void at_handler_handle_pull(void (*callback)(char *, size_t, void *ctx), void *ctx, at_handler_t *handler) {
  for (size_t i = 0; i < at_util_vector_size(handler->frames); i++) {
    sx127x_frame_t *cur_frame = NULL;
    at_util_vector_get(i, (void *) &cur_frame, handler->frames);
    int code = at_util_hex2string(cur_frame->data, cur_frame->data_length, handler->message);
    if (code != 0) {
      at_handler_respond(handler, callback, ctx, "unable to convert to hex\r\n");
      sx127x_util_frame_destroy(cur_frame);
      continue;
    }
    at_handler_respond(handler, callback, ctx, "%s,%d,%g,%d,%" PRIu64 "\r\n", handler->message, cur_frame->rssi, cur_frame->snr, cur_frame->frequency_error, cur_frame->timestamp);
    sx127x_util_frame_destroy(cur_frame);
  }
  at_util_vector_clear(handler->frames);
  at_handler_respond(handler, callback, ctx, "OK\r\n");
}

esp_err_t at_handler_add_frame(sx127x_frame_t *frame, at_handler_t *handler) {
  return at_util_vector_add(frame, handler->frames);
}

void at_handler_process(char *input, size_t input_length, void (*callback)(char *, size_t, void *ctx), void *ctx, at_handler_t *handler) {
  if (strcmp("AT", input) == 0) {
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    return;
  }
  if (strcmp("AT+GMR", input) == 0) {
    //Should be taken from esp-idf default app version?
    at_handler_respond(handler, callback, ctx, "2.0\r\nOK\r\n");
    return;
  }
  if (strcmp("AT+STATE", input) == 0) {
    uint8_t registers[0x80];
    sx127x_dump_registers(registers, handler->device->device);
    for (int i = 0; i < sizeof(registers); i++) {
      if (i != 0) {
        printf(",");
      }
      printf("0x%x", registers[i]);
    }
    printf("\n");
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    return;
  }
  if (strcmp("AT+RESET", input) == 0) {
    esp_err_t code = sx127x_util_reset();
    if (code != ESP_OK) {
      at_handler_respond(handler, callback, ctx, "Unable to reset sx127x chip: %s\r\nERROR\r\n", esp_err_to_name(code));
    } else {
      at_handler_respond(handler, callback, ctx, "OK\r\n");
    }
    return;
  }
  if (strcmp("AT+DISPLAY?", input) == 0) {
    at_handler_respond(handler, callback, ctx, "%d\r\nOK\r\n", (handler->at_config->init_display ? 1 : 0));
    return;
  }
  if (strcmp("AT+TIME?", input) == 0) {
    time_t timer = time(NULL);
    struct tm *tm_info = localtime(&timer);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    at_handler_respond(handler, callback, ctx, "%s\r\nOK\r\n", buffer);
    return;
  }
  if (strcmp("AT+MINFREQ?", input) == 0) {
    at_handler_respond(handler, callback, ctx, "%" PRIu64 "\r\nOK\r\n", sx127x_util_get_min_frequency());
    return;
  }
  if (strcmp("AT+MAXFREQ?", input) == 0) {
    at_handler_respond(handler, callback, ctx, "%" PRIu64 "\r\nOK\r\n", sx127x_util_get_max_frequency());
    return;
  }
  if (strcmp("AT+BLUETOOTH?", input) == 0) {
    if (handler->at_config->bt_address != NULL) {
      esp_err_t code = at_util_hex2string(handler->at_config->bt_address, sizeof(uint8_t) * BT_ADDRESS_LENGTH, handler->message);
      if (code != ESP_OK) {
        at_handler_respond(handler, callback, ctx, "Unable to convert MAC address to string: %s\r\n", esp_err_to_name(code));
      } else {
        at_handler_respond(handler, callback, ctx, "Server: %.2s:%.2s:%.2s:%.2s:%.2s:%.2s\r\n", handler->message, handler->message + 2, handler->message + 4, handler->message + 6, handler->message + 8, handler->message + 10);
      }
    }
    uint8_t mac[BT_ADDRESS_LENGTH];
    esp_err_t code = esp_read_mac(mac, ESP_MAC_BT);
    if (code != ESP_OK) {
      at_handler_respond(handler, callback, ctx, "Unable to read bluetooth address: %s\r\nERROR\r\n", esp_err_to_name(code));
    } else {
      code = at_util_hex2string(mac, sizeof(mac), handler->message);
      if (code != ESP_OK) {
        at_handler_respond(handler, callback, ctx, "Unable to convert MAC address to string: %s\r\nERROR\r\n", esp_err_to_name(code));
      } else {
        at_handler_respond(handler, callback, ctx, "LoraAt: %.2s:%.2s:%.2s:%.2s:%.2s:%.2s\r\nOK\r\n", handler->message, handler->message + 2, handler->message + 4, handler->message + 6, handler->message + 8, handler->message + 10);
      }
    }
    return;
  }
  if (strcmp("AT+DSCONFIG=", input) == 0) {
    ERROR_CHECK("unable to stop timer", at_timer_stop(handler->timer));
    ERROR_CHECK("unable to save config", lora_at_config_set_dsconfig(0, 0, handler->at_config));
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    return;
  }
  if (strcmp("AT+BLUETOOTH=", input) == 0) {
    ERROR_CHECK("unable to save config", lora_at_config_set_bt_address(NULL, 0, handler->at_config));
    ble_client_disconnect(handler->bluetooth);
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    return;
  }
  if (strcmp("AT+STOPRX", input) == 0) {
    ERROR_CHECK("unable to stop RX", sx127x_util_stop_rx(handler->device));
    at_handler_handle_pull(callback, ctx, handler);
    ERROR_CHECK("unable to set display status", lora_at_display_set_status("IDLE", handler->display));
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
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    return;
  }
  uint64_t inactivity_period_millis;
  uint64_t deep_sleep_period_millis;
  matched = sscanf(input, "AT+DSCONFIG=%" PRIu64 ",%" PRIu64, &inactivity_period_millis, &deep_sleep_period_millis);
  if (matched == 2) {
    ERROR_CHECK("unable to start timer", at_timer_start(inactivity_period_millis * 1000, handler->timer));
    ERROR_CHECK("unable to save config", lora_at_config_set_dsconfig(inactivity_period_millis * 1000, deep_sleep_period_millis * 1000, handler->at_config));
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    return;
  }
  uint64_t time;
  matched = sscanf(input, "AT+TIME=%" PRIu64, &time);
  if (matched == 1) {
    struct timeval now;
    now.tv_sec = (time_t) (time / 1000);
    now.tv_usec = (suseconds_t) ((time % 1000) * 1000);
    int code = settimeofday(&now, NULL);
    if (code != 0) {
      at_handler_respond(handler, callback, ctx, "%s\r\nERROR\r\n", strerror(errno));
    } else {
      at_handler_respond(handler, callback, ctx, "OK\r\n");
    }
    return;
  }
  lora_config_t state;
  matched = sscanf(input, "AT+LORARX=%" PRIu64 ",%" PRIu32 ",%hhu,%hhu,%hhu,%hu,%hhu,%hhu,%hhu,%hhu,%hhu", &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.preambleLength, &state.gain, &state.ldo, &state.useCrc, &state.useExplicitHeader, &state.length);
  if (matched == 11) {
    ERROR_CHECK("unable to rx", sx127x_util_lora_rx(SX127x_MODE_RX_CONT, &state, handler->device));
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    lora_at_display_set_status("RX", handler->display);
    return;
  }

  memset(handler->message, '\0', sizeof(handler->message));
  matched = sscanf(input, "AT+LORACADRX=%" PRIu64 ",%" PRIu32 ",%hhu,%hhu,%hhu,%hu,%hhu,%hhu,%hhu,%hhu,%hhu", &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.preambleLength, &state.gain, &state.ldo, &state.useCrc, &state.useExplicitHeader, &state.length);
  if (matched == 11) {
    ERROR_CHECK("unable to cadrx", sx127x_util_lora_rx(SX127x_MODE_CAD, &state, handler->device));
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    lora_at_display_set_status("CAD", handler->display);
    return;
  }

  memset(handler->message, '\0', sizeof(handler->message));
  matched = sscanf(input, "AT+LORATX=%[^,],%" PRIu64 ",%" PRIu32 ",%hhu,%hhu,%hhu,%hu,%hhu,%hhu,%hhu,%hhu,%hhd,%hd,%hhu", handler->message, &state.freq, &state.bw, &state.sf, &state.cr, &state.syncWord, &state.preambleLength, &state.ldo, &state.useCrc, &state.useExplicitHeader, &state.length,
                   &state.power, &state.ocp, &state.pin);
  if (matched == 14) {
    ERROR_CHECK("unable to convert HEX to byte array", at_util_string2hex(handler->message, handler->message_hex, &handler->message_hex_length));
    lora_at_display_set_status("TX", handler->display);
    esp_err_t code = sx127x_util_lora_tx(handler->message_hex, handler->message_hex_length, &state, handler->device);
    if (code != ESP_OK) {
      lora_at_display_set_status("IDLE", handler->display);
      at_handler_respond(handler, callback, ctx, "unable to tx: %s\r\nERROR\r\n", esp_err_to_name(code));
      return;
    }
    // will be sent from tx callback when message was actually sent
    // this will allow client applications to send next message
    // only when the previous was sent
    // callback("OK\r\n", ctx);
    return;
  }

  fsk_config_t fsk_config;
  memset(handler->syncword, '\0', sizeof(handler->syncword));
  matched = sscanf(input, "AT+FSKRX=%" PRIu64 ",%" PRIu32 ",%" PRIu32 ",%hu,%[^,],%hhu,%hhu,%hhu,%" PRIu32 ",%" PRIu32, &fsk_config.freq, &fsk_config.bitrate, &fsk_config.freq_deviation, &fsk_config.preamble, handler->syncword, &fsk_config.encoding, &fsk_config.data_shaping, &fsk_config.crc,
                   &fsk_config.rx_bandwidth, &fsk_config.rx_afc_bandwidth);
  if (matched == 10) {
    ERROR_CHECK("unable to convert HEX to byte array", at_util_string2hex(handler->syncword, handler->syncword_hex, &handler->syncword_hex_length));
    fsk_config.syncword = handler->syncword_hex;
    fsk_config.syncword_length = handler->syncword_hex_length;
    ERROR_CHECK("unable to rx", sx127x_util_fsk_rx(&fsk_config, handler->device));
    at_handler_respond(handler, callback, ctx, "OK\r\n");
    lora_at_display_set_status("RX", handler->display);
    return;
  }

  memset(handler->message, '\0', sizeof(handler->message));
  memset(handler->syncword, '\0', sizeof(handler->syncword));
  matched = sscanf(input, "AT+FSKTX=%[^,],%" PRIu64 ",%" PRIu32 ",%" PRIu32 ",%hu,%[^,],%hhu,%hhu,%hhu,%hhd,%hd,%hhu", handler->message, &fsk_config.freq, &fsk_config.bitrate, &fsk_config.freq_deviation, &fsk_config.preamble, handler->syncword, &fsk_config.encoding, &fsk_config.data_shaping,
                   &fsk_config.crc, &fsk_config.power, &fsk_config.ocp, &fsk_config.pin);
  if (matched == 12) {
    ERROR_CHECK("unable to convert HEX to byte array", at_util_string2hex(handler->syncword, handler->syncword_hex, &handler->syncword_hex_length));
    fsk_config.syncword = handler->syncword_hex;
    fsk_config.syncword_length = handler->syncword_hex_length;
// set tx before actually running function because if the race with tx_Callback
    ERROR_CHECK("unable to convert HEX to byte array", at_util_string2hex(handler->message, handler->message_hex, &handler->message_hex_length));
    lora_at_display_set_status("TX", handler->display);
    esp_err_t code = sx127x_util_fsk_tx(handler->message_hex, handler->message_hex_length, &fsk_config, handler->device);
    if (code != ESP_OK) {
      lora_at_display_set_status("IDLE", handler->display);
      at_handler_respond(handler, callback, ctx, "unable to tx: %s\r\nERROR\r\n", esp_err_to_name(code));
      return;
    }
// will be sent from tx callback when message was actually sent
// this will allow client applications to send next message
// only when the previous was sent
// callback("OK\r\n", ctx);
    return;
  }

  memset(handler->message, '\0', sizeof(handler->message));
  matched = sscanf(input, "AT+BLUETOOTH=%[^,]", handler->message);
  if (matched == 1) {
    size_t message_length = strlen(handler->message);
    if (message_length == 17) {
      ERROR_CHECK("unable to convert address to hex", at_util_string2hex(handler->message, handler->message_hex, &handler->message_hex_length));
      ERROR_CHECK("unable to save config", lora_at_config_set_bt_address(handler->message_hex, handler->message_hex_length, handler->at_config));
      at_handler_respond(handler, callback, ctx, "OK\r\n");
    } else {
      at_handler_respond(handler, callback, ctx, "invalid address format. expected: 00:00:00:00:00:00\r\nERROR\r\n");
    }
    return;
  }
  at_handler_respond(handler, callback, ctx, "unknown command\r\nERROR\r\n");
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