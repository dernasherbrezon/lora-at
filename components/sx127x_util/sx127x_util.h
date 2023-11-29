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

#pragma pack(push, 1)
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
  uint8_t ldo;
  uint8_t useCrc;
  uint8_t useExplicitHeader;
  uint8_t length;
} lora_config_t;

typedef struct {
  uint64_t startTimeMillis;
  uint64_t endTimeMillis;
  uint64_t currentTimeMillis;
  uint64_t freq;
  uint32_t bitrate;
  uint32_t freq_deviation;
  uint16_t preamble;
  uint8_t *syncword;
  uint8_t syncword_length;
  uint8_t encoding;
  uint8_t data_shaping;
  uint8_t crc; // 0 - off, 1 - ccitt, 2 - ibm
  int8_t power;
  uint32_t rx_bandwidth;
  uint32_t rx_afc_bandwidth;
} fsk_config_t;
#pragma pack(pop)

esp_err_t sx127x_util_init(sx127x **device);

esp_err_t sx127x_util_read_frame(sx127x *device, uint8_t *data, uint16_t data_length, sx127x_modulation_t active_mode, lora_frame_t **result);

esp_err_t sx127x_util_lora_rx(sx127x_mode_t opmod, lora_config_t *req, sx127x *device);

esp_err_t sx127x_util_reset();

esp_err_t sx127x_util_lora_tx(uint8_t *data, uint8_t data_length, lora_config_t *req, sx127x *device);

esp_err_t sx127x_util_fsk_rx(fsk_config_t *req, sx127x *device);

esp_err_t sx127x_util_fsk_tx(uint8_t *data, size_t data_length, fsk_config_t *req, sx127x *device);

void sx127x_util_frame_destroy(lora_frame_t *frame);

uint64_t sx127x_util_get_min_frequency();

uint64_t sx127x_util_get_max_frequency();

#endif //LORA_AT_SX127X_UTIL_H
