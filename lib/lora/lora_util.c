#include "lora_util.h"

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <stdlib.h>
#include <string.h>

// defined in platformio.ini
#ifndef PIN_CS
#define PIN_CS 18
#endif
#ifndef PIN_MOSI
#define PIN_MOSI 27
#endif
#ifndef PIN_MISO
#define PIN_MISO 19
#endif
#ifndef PIN_SCK
#define PIN_SCK 5
#endif

esp_err_t lora_util_init(sx127x **device) {
  spi_bus_config_t config = {
      .mosi_io_num = PIN_MOSI,
      .miso_io_num = PIN_MISO,
      .sclk_io_num = PIN_SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0,
  };
  esp_err_t code = spi_bus_initialize(HSPI_HOST, &config, 1);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_create(HSPI_HOST, PIN_CS, device);
  if (code != ESP_OK) {
    return code;
  }
  return ESP_OK;
}

esp_err_t lora_util_start_common(rx_request *request, sx127x *device) {
  esp_err_t code = sx127x_set_opmod(SX127x_MODE_SLEEP, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_frequency(request->freq * 1E6, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_reset_fifo(device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_lna_boost_hf(SX127x_LNA_BOOST_HF_ON, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_opmod(SX127x_MODE_STANDBY, device);
  if (code != ESP_OK) {
    return code;
  }
  sx127x_bw_t bw;
  if (request->bw == 7.8) {
    bw = SX127x_BW_7800;
  } else if (request->bw == 10.4) {
    bw = SX127x_BW_10400;
  } else if (request->bw == 15.6) {
    bw = SX127x_BW_15600;
  } else if (request->bw == 20.8) {
    bw = SX127x_BW_20800;
  } else if (request->bw == 31.25) {
    bw = SX127x_BW_31250;
  } else if (request->bw == 41.7) {
    bw = SX127x_BW_41700;
  } else if (request->bw == 62.5) {
    bw = SX127x_BW_62500;
  } else if (request->bw == 125.0) {
    bw = SX127x_BW_125000;
  } else if (request->bw == 250.0) {
    bw = SX127x_BW_250000;
  } else if (request->bw == 500.0) {
    bw = SX127x_BW_500000;
  } else {
    return -1;
  }
  code = sx127x_set_bandwidth(bw, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_implicit_header(NULL, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_modem_config_2((sx127x_sf_t)(request->sf << 4), device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_syncword(request->syncWord, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_preamble_length(request->preambleLength, device);
  if (code != ESP_OK) {
    return code;
  }
  // force ldo settings
  if (request->ldo == LDO_ON) {
    code = sx127x_set_low_datarate_optimization(SX127x_LOW_DATARATE_OPTIMIZATION_ON, device);
  } else if (request->ldo == LDO_OFF) {
    code = sx127x_set_low_datarate_optimization(SX127x_LOW_DATARATE_OPTIMIZATION_OFF, device);
  }
  return code;
}

esp_err_t lora_util_start_rx(rx_request *request, sx127x *device) {
  esp_err_t code = lora_util_start_common(request, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_lna_gain((sx127x_gain_t)(request->gain << 5), device);
  if (code != ESP_OK) {
    return code;
  }
  return sx127x_set_opmod(SX127x_MODE_RX_CONT, device);
}

esp_err_t lora_util_start_tx(uint8_t *data, size_t data_length, rx_request *request, sx127x *device) {
  esp_err_t code = lora_util_start_common(request, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_pa_config(SX127x_PA_PIN_BOOST, request->power, device);
  if (code != ESP_OK) {
    return code;
  }
  sx127x_tx_header_t header;
  header.crc = SX127x_RX_PAYLOAD_CRC_ON;
  header.coding_rate = ((sx127x_cr_t)(request->cr - 4)) << 1;
  code = sx127x_set_tx_explicit_header(&header, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx127x_set_for_transmission(data, data_length, device);
  if (code != ESP_OK) {
    return code;
  }
  return sx127x_set_opmod(SX127x_MODE_TX, device);
}

esp_err_t lora_util_read_frame(sx127x *device, lora_frame **frame) {
  uint8_t *data = NULL;
  uint8_t data_length = 0;
  esp_err_t code = sx127x_read_payload(device, &data, &data_length);
  if (code != ESP_OK) {
    return code;
  }
  if (data_length == 0) {
    // no message received
    *frame = NULL;
    return ESP_OK;
  }
  int16_t rssi;
  code = sx127x_get_packet_rssi(device, &rssi);
  if (code != ESP_OK) {
    return code;
  }
  float snr;
  code = sx127x_get_packet_snr(device, &snr);
  if (code != ESP_OK) {
    return code;
  }
  int32_t frequency_error;
  code = sx127x_get_frequency_error(device, &frequency_error);
  if (code != ESP_OK) {
    return code;
  }
  uint8_t *payload = malloc(sizeof(uint8_t) * data_length);
  if (payload == NULL) {
    return ESP_ERR_NO_MEM;
  }
  lora_frame *result = malloc(sizeof(struct lora_frame_t));
  if (result == NULL) {
    free(payload);
    return ESP_ERR_NO_MEM;
  }
  *result = (struct lora_frame_t){0};
  result->data_length = data_length;
  result->rssi = rssi;
  result->snr = snr;
  result->frequency_error = frequency_error;
  memcpy(payload, data, data_length * sizeof(uint8_t));
  result->data = payload;

  *frame = result;
  return ESP_OK;
}

void lora_util_frame_destroy(lora_frame *frame) {
  if (frame == NULL) {
    return;
  }
  if (frame->data != NULL) {
    free(frame->data);
  }
  free(frame);
}