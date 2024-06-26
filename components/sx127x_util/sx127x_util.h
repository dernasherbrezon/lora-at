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
} sx127x_frame_t;

typedef enum {
  LDO_AUTO = 0,
  LDO_ON = 1,
  LDO_OFF = 2
} ldo_type_t;

#pragma pack(push, 1)
typedef struct {
  uint8_t protocol_version;
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
  int16_t ocp;
  uint8_t pin;
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
  int16_t ocp;
  uint8_t pin;
} fsk_config_t;
#pragma pack(pop)

typedef struct {
  sx127x *device;
  sx127x_modulation_t modulation;
  sx127x_mode_t mode;
  int8_t temperature;
} sx127x_wrapper;

esp_err_t sx127x_util_init(sx127x_wrapper **device);

esp_err_t sx127x_util_read_frame(sx127x_wrapper *device, uint8_t *data, uint16_t data_length, sx127x_frame_t **result);

esp_err_t sx127x_util_read_temperature(sx127x_wrapper *device, int8_t *temperature);

esp_err_t sx127x_util_lora_rx(sx127x_mode_t opmod, lora_config_t *req, sx127x_wrapper *device);

esp_err_t sx127x_util_reset();

esp_err_t sx127x_util_lora_tx(uint8_t *data, uint8_t data_length, lora_config_t *req, sx127x_wrapper *device);

esp_err_t sx127x_util_fsk_rx(fsk_config_t *req, sx127x_wrapper *device);

esp_err_t sx127x_util_fsk_tx(uint8_t *data, size_t data_length, fsk_config_t *req, sx127x_wrapper *device);

esp_err_t sx127x_util_stop_rx(sx127x_wrapper *device);

esp_err_t sx127x_util_deep_sleep_enter(sx127x_wrapper *device);

void sx127x_util_frame_destroy(sx127x_frame_t *frame);

uint64_t sx127x_util_get_min_frequency();

uint64_t sx127x_util_get_max_frequency();

void sx127x_util_log_request(lora_config_t *req);

#endif //LORA_AT_SX127X_UTIL_H
