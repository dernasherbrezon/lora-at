#include "sx1278.h"

#include <stdint.h>
#include <stdlib.h>

// registers
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_OCP 0x0b
#define REG_LNA 0x0c
#define REG_FIFO_ADDR_PTR 0x0d
#define REG_FIFO_TX_BASE_ADDR 0x0e
#define REG_FIFO_RX_BASE_ADDR 0x0f
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_PKT_SNR_VALUE 0x19
#define REG_PKT_RSSI_VALUE 0x1a
#define REG_RSSI_VALUE 0x1b
#define REG_MODEM_CONFIG_1 0x1d
#define REG_MODEM_CONFIG_2 0x1e
#define REG_PREAMBLE_MSB 0x20
#define REG_PREAMBLE_LSB 0x21
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG_3 0x26
#define REG_FREQ_ERROR_MSB 0x28
#define REG_FREQ_ERROR_MID 0x29
#define REG_FREQ_ERROR_LSB 0x2a
#define REG_RSSI_WIDEBAND 0x2c
#define REG_DETECTION_OPTIMIZE 0x31
#define REG_INVERTIQ 0x33
#define REG_DETECTION_THRESHOLD 0x37
#define REG_SYNC_WORD 0x39
#define REG_INVERTIQ2 0x3b
#define REG_DIO_MAPPING_1 0x40
#define REG_DIO_MAPPING_2 0x41
#define REG_VERSION 0x42
#define REG_PA_DAC 0x4d

#define SX1278_VERSION 0x12

#define SX1278_LORA_MODE_FSK 0b00000000
#define SX1278_LORA_MODE_LORA 0b10000000

#define SX1278_OSCILLATOR_FREQUENCY 32000000
#define SX1278_FREQ_ERROR_FACTOR ((1 << 24) / SX1278_OSCILLATOR_FREQUENCY)
#define SX1278_REG_MODEM_CONFIG_3_AGC_ON 0b00000100
#define SX1278_REG_MODEM_CONFIG_3_AGC_OFF 0b00000000

#define SX1278_IRQ_FLAG_RXTIMEOUT 0b10000000
#define SX1278_IRQ_FLAG_RXDONE 0b01000000
#define SX1278_IRQ_FLAG_PAYLOAD_CRC_ERROR 0b00100000
#define SX1278_IRQ_FLAG_VALID_HEADER 0b00010000
#define SX1278_IRQ_FLAG_TXDONE 0b00001000
#define SX1278_IRQ_FLAG_CADDONE 0b00000100
#define SX1278_IRQ_FLAG_FHSSCHANGECHANNEL 0b00000010
#define SX1278_IRQ_FLAG_CAD_DETECTED 0b00000001

#define RF_MID_BAND_THRESHOLD 525E6
#define RSSI_OFFSET_HF_PORT 157
#define RSSI_OFFSET_LF_PORT 164

#define SX1278_MAX_POWER 0b01110000
#define SX1278_LOW_POWER 0b00000000

#define SX1278_HIGH_POWER_ON 0b10000111
#define SX1278_HIGH_POWER_OFF 0b10000100

typedef enum {
  SX1278_HEADER_MODE_EXPLICIT = 0b00000000,
  SX1278_HEADER_MODE_IMPLICIT = 0b00000001
} sx1278_header_mode_t;

struct sx1278_t {
  spi_device_handle_t spi;
  sx1278_implicit_header_t *header;
  uint8_t version;
  uint64_t frequency;

  // FIXME better use tasks and queues
  // see xQueueSendFromISR or similar
  volatile uint8_t irq_received;
  uint8_t packet[256];
};

esp_err_t sx1278_read_registers(int reg, sx1278 *device, size_t data_length, uint32_t *result) {
  if (data_length == 0 || data_length > 4) {
    return ESP_ERR_INVALID_ARG;
  }
  *result = 0;
  spi_transaction_t t = {
      .addr = reg & 0x7F,
      .rx_buffer = NULL,
      .tx_buffer = NULL,
      .rxlength = data_length * 8,
      .length = data_length * 8,
      .flags = SPI_TRANS_USE_RXDATA};
  esp_err_t code = spi_device_polling_transmit(device->spi, &t);
  if (code != ESP_OK) {
    return code;
  }
  for (int i = 0; i < data_length; i++) {
    *result = ((*result) << 8);
    *result = (*result) + t.rx_data[i];
  }
  return ESP_OK;
}

esp_err_t sx1278_read_register(int reg, sx1278 *device, uint8_t *result) {
  uint32_t value;
  esp_err_t code = sx1278_read_registers(reg, device, 1, &value);
  if (code != ESP_OK) {
    return code;
  }
  *result = (uint8_t)value;
  return ESP_OK;
}

esp_err_t sx1278_write_register(int reg, uint8_t *data, size_t data_length, sx1278 *device) {
  if (data_length == 0 || data_length > 4) {
    return ESP_ERR_INVALID_ARG;
  }
  spi_transaction_t t = {
      .addr = reg | 0x80,
      .rx_buffer = NULL,
      .tx_buffer = NULL,
      .rxlength = data_length * 8,
      .length = data_length * 8,
      .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA};
  for (int i = 0; i < data_length; i++) {
    t.tx_data[i] = data[i];
  }
  return spi_device_polling_transmit(device->spi, &t);
}

esp_err_t sx1278_append_register(int reg, uint8_t value, uint8_t mask, sx1278 *device) {
  uint8_t previous = 0;
  esp_err_t code = sx1278_read_register(reg, device, &previous);
  if (code != ESP_OK) {
    return code;
  }
  uint8_t data[] = {(previous & mask) | value};
  return sx1278_write_register(reg, data, 1, device);
}

esp_err_t sx1278_set_low_datarate_optimization(sx1278_low_datarate_optimization_t value, sx1278 *device) {
  return sx1278_append_register(REG_MODEM_CONFIG_3, value, 0b11110111, device);
}

esp_err_t sx1278_get_bandwidth(sx1278 *device, uint32_t *bandwidth) {
  uint8_t config = 0;
  esp_err_t code = sx1278_read_register(REG_MODEM_CONFIG_1, device, &config);
  if (code != ESP_OK) {
    return code;
  }
  config = (config >> 4);
  switch (config) {
    case 0b0000:
      *bandwidth = 7800;
      break;
    case 0b0001:
      *bandwidth = 10400;
      break;
    case 0b0010:
      *bandwidth = 15600;
      break;
    case 0b0011:
      *bandwidth = 20800;
      break;
    case 0b0100:
      *bandwidth = 31250;
      break;
    case 0b0101:
      *bandwidth = 41700;
      break;
    case 0b0110:
      *bandwidth = 62500;
      break;
    case 0b0111:
      *bandwidth = 125000;
      break;
    case 0b1000:
      *bandwidth = 250000;
      break;
    case 0b1001:
      *bandwidth = 500000;
      break;
    default:
      return ESP_ERR_INVALID_ARG;
  }
  return ESP_OK;
}

esp_err_t sx1278_reload_low_datarate_optimization(sx1278 *device) {
  uint32_t bandwidth;
  esp_err_t code = sx1278_get_bandwidth(device, &bandwidth);
  if (code != ESP_OK) {
    return code;
  }
  uint8_t config = 0;
  code = sx1278_read_register(REG_MODEM_CONFIG_2, device, &config);
  if (code != ESP_OK) {
    return code;
  }
  config = (config >> 4);

  // Section 4.1.1.5
  long symbol_duration = 1000 / (bandwidth / (1L << config));
  if (symbol_duration > 16) {
    // force low data rate optimization
    return sx1278_set_low_datarate_optimization(SX1278_LOW_DATARATE_OPTIMIZATION_ON, device);
  }
  return ESP_OK;
}

esp_err_t sx1278_create(spi_host_device_t host, int cs, sx1278 **result) {
  struct sx1278_t *device = malloc(sizeof(struct sx1278_t));
  if (device == NULL) {
    return ESP_ERR_NO_MEM;
  }
  *device = (struct sx1278_t){0};
  spi_device_interface_config_t dev_cfg = {
      .clock_speed_hz = 8E6,
      .spics_io_num = cs,
      .queue_size = 16,
      .command_bits = 0,
      .address_bits = 8,
      .dummy_bits = 0,
      .mode = 0};
  esp_err_t code = spi_bus_add_device(host, &dev_cfg, &device->spi);
  if (code != ESP_OK) {
    sx1278_destroy(device);
    return code;
  }

  code = sx1278_read_register(REG_VERSION, device, &device->version);
  if (code != ESP_OK) {
    sx1278_destroy(device);
    return code;
  }
  if (device->version != SX1278_VERSION) {
    sx1278_destroy(device);
    return ESP_ERR_INVALID_VERSION;
  }
  *result = device;
  return ESP_OK;
}

esp_err_t sx1278_set_opmod(sx1278_mode_t opmod, sx1278 *device) {
  uint8_t data[] = {0};
  // enforce DIO mappings for during RX and TX
  if (opmod == SX1278_MODE_RX_CONT || opmod == SX1278_MODE_RX_SINGLE) {
    esp_err_t code = sx1278_append_register(REG_DIO_MAPPING_1, SX1278_DIO0_RX_DONE, 0b00111111, device);
    if (code != ESP_OK) {
      return code;
    }
  } else if (opmod == SX1278_MODE_TX) {
    esp_err_t code = sx1278_append_register(REG_DIO_MAPPING_1, SX1278_DIO0_TX_DONE, 0b00111111, device);
    if (code != ESP_OK) {
      return code;
    }
  }
  data[0] = (opmod | SX1278_LORA_MODE_LORA);
  return sx1278_write_register(REG_OP_MODE, data, 1, device);
}

esp_err_t sx1278_set_frequency(uint64_t frequency, sx1278 *device) {
  uint64_t adjusted = (frequency << 19) / SX1278_OSCILLATOR_FREQUENCY;
  uint8_t data[] = {(uint8_t)(adjusted >> 16), (uint8_t)(adjusted >> 8), (uint8_t)(adjusted >> 0)};
  esp_err_t result = sx1278_write_register(REG_FRF_MSB, data, 3, device);
  if (result != ESP_OK) {
    return result;
  }
  device->frequency = frequency;
  return ESP_OK;
}

esp_err_t sx1278_reset_fifo(sx1278 *device) {
  // reset both RX and TX
  uint8_t data[] = {0, 0};
  return sx1278_write_register(REG_FIFO_TX_BASE_ADDR, data, 2, device);
}

esp_err_t sx1278_set_lna_gain(sx1278_gain_t gain, sx1278 *device) {
  if (gain == SX1278_LNA_GAIN_AUTO) {
    return sx1278_append_register(REG_MODEM_CONFIG_3, SX1278_REG_MODEM_CONFIG_3_AGC_ON, 0b11111011, device);
  }
  esp_err_t code = sx1278_append_register(REG_MODEM_CONFIG_3, SX1278_REG_MODEM_CONFIG_3_AGC_OFF, 0b11111011, device);
  if (code != ESP_OK) {
    return code;
  }
  return sx1278_append_register(REG_LNA, gain, 0b00011111, device);
}

esp_err_t sx1278_set_lna_boost_hf(sx1278_lna_boost_hf_t value, sx1278 *device) {
  return sx1278_append_register(REG_LNA, value, 0b11111100, device);
}

esp_err_t sx1278_set_bandwidth(sx1278_bw_t bandwidth, sx1278 *device) {
  esp_err_t code = sx1278_append_register(REG_MODEM_CONFIG_1, bandwidth, 0b00001111, device);
  if (code != ESP_OK) {
    return code;
  }
  return sx1278_reload_low_datarate_optimization(device);
}

esp_err_t sx1278_set_modem_config_2(sx1278_sf_t spreading_factor, sx1278 *device) {
  if (spreading_factor == SX1278_SF_6 && device->header == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  uint8_t detection_optimize;
  uint8_t detection_threshold;
  if (spreading_factor == SX1278_SF_6) {
    detection_optimize = 0xc5;
    detection_threshold = 0x0c;
    // make header implicit
  } else {
    detection_optimize = 0xc3;
    detection_threshold = 0x0a;
  }
  uint8_t data[] = {detection_optimize};
  esp_err_t code = sx1278_write_register(REG_DETECTION_OPTIMIZE, data, 1, device);
  if (code != ESP_OK) {
    return code;
  }
  data[0] = detection_threshold;
  code = sx1278_write_register(REG_DETECTION_THRESHOLD, data, 1, device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_append_register(REG_MODEM_CONFIG_2, spreading_factor, 0b00001111, device);
  if (code != ESP_OK) {
    return code;
  }
  return sx1278_reload_low_datarate_optimization(device);
}

esp_err_t sx1278_set_syncword(uint8_t value, sx1278 *device) {
  uint8_t data[] = {value};
  return sx1278_write_register(REG_SYNC_WORD, data, 1, device);
}

esp_err_t sx1278_set_preamble_length(uint16_t value, sx1278 *device) {
  uint8_t data[] = {(uint8_t)(value >> 8), (uint8_t)(value >> 0)};
  return sx1278_write_register(REG_PREAMBLE_MSB, data, 2, device);
}

esp_err_t sx1278_set_implicit_header(sx1278_implicit_header_t *header, sx1278 *device) {
  device->header = header;
  if (header == NULL) {
    return sx1278_append_register(REG_MODEM_CONFIG_1, SX1278_HEADER_MODE_EXPLICIT, 0b11111110, device);
  } else {
    esp_err_t code = sx1278_append_register(REG_MODEM_CONFIG_1, SX1278_HEADER_MODE_IMPLICIT | device->header->coding_rate, 0b11110000, device);
    if (code != ESP_OK) {
      return code;
    }
    return sx1278_append_register(REG_MODEM_CONFIG_2, header->crc, 0b11111011, device);
  }
}

void IRAM_ATTR sx1278_handle_interrupt(void *arg) {
  sx1278 *device = (sx1278 *)arg;
  device->irq_received = 1;
}

esp_err_t sx1278_receive(sx1278 *device, uint8_t **packet, uint8_t *packet_length) {
  if (!device->irq_received) {
    *packet_length = 0;
    *packet = NULL;
    return ESP_OK;
  }
  device->irq_received = 0;

  uint8_t value;
  esp_err_t code = sx1278_read_register(REG_IRQ_FLAGS, device, &value);
  if (code != ESP_OK) {
    *packet_length = 0;
    *packet = NULL;
    return code;
  }
  // clear the irq
  uint8_t data[] = {value};
  code = sx1278_write_register(REG_IRQ_FLAGS, data, 1, device);
  if (code != ESP_OK) {
    *packet_length = 0;
    *packet = NULL;
    return code;
  }
  if ((value & SX1278_IRQ_FLAG_RXDONE) == 0) {
    *packet_length = 0;
    *packet = NULL;
    // ignore non rx-related interrupts
    return ESP_OK;
  }
  uint8_t length;
  if (device->header == NULL) {
    code = sx1278_read_register(REG_RX_NB_BYTES, device, &length);
    if (code != ESP_OK) {
      *packet_length = 0;
      *packet = NULL;
      return code;
    }
  } else {
    length = device->header->length;
  }

  uint8_t current;
  code = sx1278_read_register(REG_FIFO_RX_CURRENT_ADDR, device, &current);
  if (code != ESP_OK) {
    *packet_length = 0;
    *packet = NULL;
    return code;
  }
  data[0] = current;
  code = sx1278_write_register(REG_FIFO_ADDR_PTR, data, 1, device);
  if (code != ESP_OK) {
    *packet_length = 0;
    *packet = NULL;
    return code;
  }

  spi_transaction_t t = {
      .addr = REG_FIFO & 0x7F,
      .rx_buffer = device->packet,
      .tx_buffer = NULL,
      .rxlength = length * 8,
      .length = length * 8};
  code = spi_device_polling_transmit(device->spi, &t);
  if (code != ESP_OK) {
    *packet_length = 0;
    *packet = NULL;
    return code;
  }

  *packet = device->packet;
  *packet_length = length;
  return ESP_OK;
}

esp_err_t sx1278_get_packet_rssi(sx1278 *device, int16_t *rssi) {
  uint8_t value;
  esp_err_t code = sx1278_read_register(REG_PKT_RSSI_VALUE, device, &value);
  if (code != ESP_OK) {
    return code;
  }
  if (device->frequency < RF_MID_BAND_THRESHOLD) {
    *rssi = value - RSSI_OFFSET_LF_PORT;
  } else {
    *rssi = value - RSSI_OFFSET_HF_PORT;
  }
  // section 5.5.5.
  float snr;
  code = sx1278_get_packet_snr(device, &snr);
  // if snr failed then rssi is not precise
  if (code == ESP_OK && snr < 0) {
    *rssi = *rssi + snr;
  }
  return ESP_OK;
}

esp_err_t sx1278_get_packet_snr(sx1278 *device, float *snr) {
  uint8_t value;
  esp_err_t code = sx1278_read_register(REG_PKT_SNR_VALUE, device, &value);
  if (code != ESP_OK) {
    return code;
  }
  *snr = ((int8_t)value) * 0.25;
  return ESP_OK;
}

esp_err_t sx1278_get_frequency_error(sx1278 *device, int32_t *result) {
  uint32_t frequency_error;
  esp_err_t code = sx1278_read_registers(REG_FREQ_ERROR_MSB, device, 3, &frequency_error);
  if (code != ESP_OK) {
    return code;
  }
  uint32_t bandwidth;
  code = sx1278_get_bandwidth(device, &bandwidth);
  if (code != ESP_OK) {
    return code;
  }
  if (frequency_error & 0x80000) {
    frequency_error = ~frequency_error + 1;
    *result = -1;
  } else {
    *result = 1;
  }
  *result = (*result) * frequency_error * SX1278_FREQ_ERROR_FACTOR * bandwidth / 500000.0f;
  return ESP_OK;
}

esp_err_t sx1278_dump_registers(sx1278 *device) {
  uint8_t length = 0x7F;
  for (int i = 0; i < length; i++) {
    uint8_t value;
    sx1278_read_register(i, device, &value);
    printf("0x%.2x: 0x%.2x\n", i, value);
  }
  return ESP_OK;
}

esp_err_t sx1278_set_dio_mapping1(sx1278_dio_mapping1_t value, sx1278 *device) {
  uint8_t data[] = {value};
  return sx1278_write_register(REG_DIO_MAPPING_1, data, 1, device);
}

esp_err_t sx1278_set_dio_mapping2(sx1278_dio_mapping2_t value, sx1278 *device) {
  uint8_t data[] = {value};
  return sx1278_write_register(REG_DIO_MAPPING_2, data, 1, device);
}

esp_err_t sx1278_set_pa_config(sx1278_pa_pin_t pin, int power, sx1278 *device) {
  if (pin == SX1278_PA_PIN_RFO && (power < -4 || power > 15)) {
    return ESP_ERR_INVALID_ARG;
  }
  if (pin == SX1278_PA_PIN_BOOST && (power < 2 || power > 20)) {
    return ESP_ERR_INVALID_ARG;
  }
  uint8_t data[] = {0};
  if (pin == SX1278_PA_PIN_BOOST && power == 20) {
    data[0] = SX1278_HIGH_POWER_ON;
  } else {
    data[0] = SX1278_HIGH_POWER_OFF;
  }
  esp_err_t code = sx1278_write_register(REG_PA_DAC, data, 1, device);
  if (code != ESP_OK) {
    return code;
  }
  uint8_t max_current;
  // according to 2.5.1. Power Consumption
  if (pin == SX1278_PA_PIN_BOOST) {
    if (power == 20) {
      max_current = 120;
    } else {
      max_current = 87;
    }
  } else {
    if (power > 7) {
      max_current = 29;
    } else {
      max_current = 20;
    }
  }
  code = sx1278_set_ocp(SX1278_OCP_ON, max_current, device);
  if (code != ESP_OK) {
    return code;
  }
  uint8_t value;
  if (pin == SX1278_PA_PIN_RFO) {
    if (power < 0) {
      value = SX1278_LOW_POWER | (power + 4);
    } else {
      value = SX1278_MAX_POWER | power;
    }
    value = value | SX1278_PA_PIN_RFO;
  } else {
    if (power == 20) {
      value = SX1278_PA_PIN_BOOST | 0b00001111;
    } else {
      value = SX1278_PA_PIN_BOOST | (power - 2);
    }
  }
  data[0] = value;
  return sx1278_write_register(REG_PA_CONFIG, data, 1, device);
}

esp_err_t sx1278_set_ocp(sx1278_ocp_t onoff, uint8_t max_current, sx1278 *device) {
  uint8_t data[1];
  if (onoff == SX1278_OCP_OFF) {
    data[0] = SX1278_OCP_OFF;
    return sx1278_write_register(REG_OCP, data, 1, device);
  }
  // 5.4.4. Over Current Protection
  if (max_current <= 120) {
    data[0] = (max_current - 45) / 5;
  } else if (max_current <= 240) {
    data[0] = (max_current + 30) / 10;
  } else {
    data[0] = 27;
  }
  data[0] = (data[0] | onoff);
  return sx1278_write_register(REG_OCP, data, 1, device);
}

void sx1278_destroy(sx1278 *device) {
  if (device == NULL) {
    return;
  }
  if (device->spi != NULL) {
    spi_bus_remove_device(device->spi);
  }
  free(device);
}