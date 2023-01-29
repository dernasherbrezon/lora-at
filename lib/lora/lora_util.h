#ifndef lora_util_h
#define lora_util_h

#include <esp_err.h>
#include <stdint.h>
#include <sx127x.h>

#ifdef __cplusplus
extern "C" {
#endif

struct lora_frame_t {
  int32_t frequency_error;
  int16_t rssi;
  float snr;
  uint64_t timestamp;
  uint32_t data_length;
  uint8_t *data;
};

typedef struct lora_frame_t lora_frame;

typedef enum {
  LDO_AUTO = 0,
  LDO_ON = 1,
  LDO_OFF = 2
} ldo_type_t;

struct rx_request_t {
  uint64_t startTimeMillis;
  uint64_t endTimeMillis;
  uint64_t currentTimeMillis;
  float freq;
  float bw;
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
};

typedef struct rx_request_t rx_request;

esp_err_t lora_util_init(sx127x **device);

esp_err_t lora_util_read_frame(sx127x *device, lora_frame **result);

esp_err_t lora_util_start_rx(rx_request *req, sx127x *device);

esp_err_t lora_util_start_tx(uint8_t *data, size_t data_length, rx_request *req, sx127x *device);

void lora_util_frame_destroy(lora_frame *frame);

#ifdef __cplusplus
}
#endif
#endif