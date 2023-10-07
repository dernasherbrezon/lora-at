#ifndef LORA_AT_SX127X_UTIL_H
#define LORA_AT_SX127X_UTIL_H

#include <esp_err.h>
#include <stdint.h>
#include <sx127x.h>
#include <stddef.h>

typedef struct {
  int32_t frequency_error;
  int16_t rssi;
  float snr;
  uint64_t timestamp;
  uint8_t *data;
  uint16_t data_length;
} lora_frame_t;

typedef enum {
  LDO_AUTO = 0,
  LDO_ON = 1,
  LDO_OFF = 2
} ldo_type_t;

typedef struct {
  uint64_t startTimeMillis;
  uint64_t endTimeMillis;
  uint64_t currentTimeMillis;
  uint64_t freq;
  uint32_t bw;
  uint8_t sf;
  uint8_t cr;
  uint8_t syncWord;
  int8_t power;
  uint16_t preambleLength;
  uint8_t gain;
  ldo_type_t ldo;
  uint8_t useCrc;
  uint8_t useExplicitHeader;
  uint8_t length;
} rx_request_t;

esp_err_t lora_util_init(sx127x **device);

esp_err_t lora_util_read_frame(sx127x *device, uint8_t *data, uint16_t data_length, lora_frame_t **result);

esp_err_t lora_util_start_rx(rx_request_t *req, sx127x *device);

esp_err_t lora_util_start_tx(uint8_t *data, size_t data_length, rx_request_t *req, sx127x *device);

void lora_util_frame_destroy(lora_frame_t *frame);

uint64_t lora_util_get_min_frequency();

uint64_t lora_util_get_max_frequency();

#endif //LORA_AT_SX127X_UTIL_H
