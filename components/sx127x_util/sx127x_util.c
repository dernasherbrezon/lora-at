#include "sx127x_util.h"

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

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      return __err_rc;        \
    }                         \
  } while (0)

esp_err_t lora_util_init(sx127x **device) {
  spi_bus_config_t config = {
      .mosi_io_num = PIN_MOSI,
      .miso_io_num = PIN_MISO,
      .sclk_io_num = PIN_SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0,
  };
  ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &config, 1));
  spi_device_interface_config_t dev_cfg = {
      .clock_speed_hz = 3E6,
      .spics_io_num = PIN_CS,
      .queue_size = 16,
      .command_bits = 0,
      .address_bits = 8,
      .dummy_bits = 0,
      .mode = 0};
  spi_device_handle_t spi_device;
  ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &dev_cfg, &spi_device));
  ERROR_CHECK(sx127x_create(spi_device, device));
  return SX127X_OK;
}

esp_err_t lora_util_start_common(rx_request_t *request, sx127x *device) {
  ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, device));
  ERROR_CHECK(sx127x_set_frequency(request->freq * 1E6, device));
  ERROR_CHECK(sx127x_lora_reset_fifo(device));
  ERROR_CHECK(sx127x_rx_set_lna_boost_hf(true, device));
  ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_STANDBY, SX127x_MODULATION_LORA, device));
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
  ERROR_CHECK(sx127x_lora_set_bandwidth(bw, device));
  if (request->useExplicitHeader) {
    ERROR_CHECK(sx127x_lora_set_implicit_header(NULL, device));
  } else {
    sx127x_implicit_header_t header = {
        .coding_rate = ((sx127x_cr_t)(request->cr - 4)) << 1,
        .enable_crc = request->useCrc,
        .length = request->length};
    ESP_ERROR_CHECK(sx127x_lora_set_implicit_header(&header, device));
  }
  ERROR_CHECK(sx127x_lora_set_modem_config_2((sx127x_sf_t)(request->sf << 4), device));
  ERROR_CHECK(sx127x_lora_set_syncword(request->syncWord, device));
  ERROR_CHECK(sx127x_set_preamble_length(request->preambleLength, device));
  // force ldo settings
  if (request->ldo == LDO_ON) {
    return sx127x_lora_set_low_datarate_optimization(true, device);
  } else if (request->ldo == LDO_OFF) {
    return sx127x_lora_set_low_datarate_optimization(false, device);
  }
  return SX127X_OK;
}

esp_err_t lora_util_start_rx(rx_request_t *request, sx127x *device) {
  ERROR_CHECK(lora_util_start_common(request, device));
  ERROR_CHECK(sx127x_rx_set_lna_gain((sx127x_gain_t)(request->gain << 5), device));
  return sx127x_set_opmod(SX127x_MODE_RX_CONT, SX127x_MODULATION_LORA, device);
}

esp_err_t lora_util_start_tx(uint8_t *data, size_t data_length, rx_request_t *request, sx127x *device) {
  ERROR_CHECK(lora_util_start_common(request, device));
  ERROR_CHECK(sx127x_tx_set_pa_config(SX127x_PA_PIN_BOOST, request->power, device));
  if (request->useExplicitHeader) {
    sx127x_tx_header_t header = {
        .enable_crc = request->useCrc,
        .coding_rate = ((sx127x_cr_t)(request->cr - 4)) << 1
    };
    ERROR_CHECK(sx127x_lora_tx_set_explicit_header(&header, device));
  }
  ERROR_CHECK(sx127x_lora_tx_set_for_transmission(data, data_length, device));
  return sx127x_set_opmod(SX127x_MODE_TX, SX127x_MODULATION_LORA, device);
}

esp_err_t lora_util_read_frame(sx127x *device, uint8_t *data, uint16_t data_length, lora_frame_t **frame) {
  int16_t rssi;
  ERROR_CHECK(sx127x_rx_get_packet_rssi(device, &rssi));
  float snr;
  ERROR_CHECK(sx127x_lora_rx_get_packet_snr(device, &snr));
  int32_t frequency_error;
  ERROR_CHECK(sx127x_rx_get_frequency_error(device, &frequency_error));
  uint8_t *payload = malloc(sizeof(uint8_t) * data_length);
  if (payload == NULL) {
    return ESP_ERR_NO_MEM;
  }
  lora_frame_t *result = malloc(sizeof(lora_frame_t));
  if (result == NULL) {
    free(payload);
    return ESP_ERR_NO_MEM;
  }
  result->data_length = data_length;
  result->rssi = rssi;
  result->snr = snr;
  result->frequency_error = frequency_error;
  memcpy(payload, data, data_length * sizeof(uint8_t));
  result->data = payload;

  *frame = result;
  return ESP_OK;
}

void lora_util_frame_destroy(lora_frame_t *frame) {
  if (frame == NULL) {
    return;
  }
  if (frame->data != NULL) {
    free(frame->data);
  }
  free(frame);
}