#include "sx127x_util.h"

#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>

#include <driver/gpio.h>
#include <esp_intr_alloc.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sdkconfig.h>

#ifndef CONFIG_PIN_CS
#define CONFIG_PIN_CS 18
#endif
#ifndef CONFIG_PIN_MOSI
#define CONFIG_PIN_MOSI 27
#endif
#ifndef CONFIG_PIN_MISO
#define CONFIG_PIN_MISO 19
#endif
#ifndef CONFIG_PIN_SCK
#define CONFIG_PIN_SCK 5
#endif
#ifndef CONFIG_PIN_DIO0
#define CONFIG_PIN_DIO0 26
#endif
#ifndef CONFIG_MIN_FREQUENCY
#define CONFIG_MIN_FREQUENCY 25000000
#endif
#ifndef CONFIG_MAX_FREQUENCY
#define CONFIG_MAX_FREQUENCY 25000000
#endif

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      return __err_rc;        \
    }                         \
  } while (0)

static const char *TAG = "lora-at";
TaskHandle_t handle_interrupt;

void IRAM_ATTR lora_util_interrupt_fromisr(void *arg) {
  xTaskResumeFromISR(handle_interrupt);
}

void lora_util_interrupt_task(void *arg) {
  while (1) {
    vTaskSuspend(NULL);
    sx127x_handle_interrupt((sx127x *) arg);
  }
}

void setup_gpio_interrupts(gpio_num_t gpio, sx127x *device) {
  gpio_set_direction(gpio, GPIO_MODE_INPUT);
  gpio_pulldown_en(gpio);
  gpio_pullup_dis(gpio);
  gpio_set_intr_type(gpio, GPIO_INTR_POSEDGE);
  gpio_isr_handler_add(gpio, lora_util_interrupt_fromisr, (void *) device);
}

esp_err_t lora_util_init(sx127x **device) {
  spi_bus_config_t config = {
      .mosi_io_num = CONFIG_PIN_MOSI,
      .miso_io_num = CONFIG_PIN_MISO,
      .sclk_io_num = CONFIG_PIN_SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0,
  };
  ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &config, 1));
  spi_device_interface_config_t dev_cfg = {
      .clock_speed_hz = 3000000,
      .spics_io_num = CONFIG_PIN_CS,
      .queue_size = 16,
      .command_bits = 0,
      .address_bits = 8,
      .dummy_bits = 0,
      .mode = 0};
  spi_device_handle_t spi_device;
  sx127x *result = NULL;
  ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &dev_cfg, &spi_device));
  ERROR_CHECK(sx127x_create(spi_device, &result));

  BaseType_t task_code = xTaskCreatePinnedToCore(lora_util_interrupt_task, "handle interrupt", 8196, result, 2, &handle_interrupt, xPortGetCoreID());
  if (task_code != pdPASS) {
    ESP_LOGE(TAG, "can't create task %d", task_code);
    sx127x_destroy(result);
    return ESP_ERR_INVALID_STATE;
  }

  gpio_install_isr_service(0);
  setup_gpio_interrupts((gpio_num_t) CONFIG_PIN_DIO0, result);
  *device = result;
  return SX127X_OK;
}

esp_err_t lora_util_start_common(rx_request_t *request, sx127x *device) {
  ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, device));
  ERROR_CHECK(sx127x_set_frequency(request->freq, device));
  ERROR_CHECK(sx127x_lora_reset_fifo(device));
  ERROR_CHECK(sx127x_rx_set_lna_boost_hf(true, device));
  ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_STANDBY, SX127x_MODULATION_LORA, device));
  sx127x_bw_t bw;
  if (request->bw == 7800) {
    bw = SX127x_BW_7800;
  } else if (request->bw == 10400) {
    bw = SX127x_BW_10400;
  } else if (request->bw == 15600) {
    bw = SX127x_BW_15600;
  } else if (request->bw == 20800) {
    bw = SX127x_BW_20800;
  } else if (request->bw == 31250) {
    bw = SX127x_BW_31250;
  } else if (request->bw == 41700) {
    bw = SX127x_BW_41700;
  } else if (request->bw == 62500) {
    bw = SX127x_BW_62500;
  } else if (request->bw == 125000) {
    bw = SX127x_BW_125000;
  } else if (request->bw == 250000) {
    bw = SX127x_BW_250000;
  } else if (request->bw == 500000) {
    bw = SX127x_BW_500000;
  } else {
    ESP_LOGE(TAG, "unsupported bw: %" PRIu32, request->bw);
    return ESP_ERR_INVALID_ARG;
  }
  ERROR_CHECK(sx127x_lora_set_bandwidth(bw, device));
  if (request->useExplicitHeader) {
    ERROR_CHECK(sx127x_lora_set_implicit_header(NULL, device));
  } else {
    sx127x_implicit_header_t header = {
        .coding_rate = ((sx127x_cr_t) (request->cr - 4)) << 1,
        .enable_crc = request->useCrc,
        .length = request->length};
    ESP_ERROR_CHECK(sx127x_lora_set_implicit_header(&header, device));
  }
  ERROR_CHECK(sx127x_lora_set_modem_config_2((sx127x_sf_t) (request->sf << 4), device));
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
  ERROR_CHECK(sx127x_rx_set_lna_gain((sx127x_gain_t) (request->gain << 5), device));
  int result = sx127x_set_opmod(SX127x_MODE_RX_CONT, SX127x_MODULATION_LORA, device);
  if (result == SX127X_OK) {
    ESP_LOGI(TAG, "rx started on %" PRIu64, request->freq);
  }
  return result;
}

esp_err_t lora_util_start_tx(uint8_t *data, size_t data_length, rx_request_t *request, sx127x *device) {
  ERROR_CHECK(lora_util_start_common(request, device));
  ERROR_CHECK(sx127x_tx_set_pa_config(SX127x_PA_PIN_BOOST, request->power, device));
  if (request->useExplicitHeader) {
    sx127x_tx_header_t header = {
        .enable_crc = request->useCrc,
        .coding_rate = ((sx127x_cr_t) (request->cr - 4)) << 1
    };
    ERROR_CHECK(sx127x_lora_tx_set_explicit_header(&header, device));
  }
  ERROR_CHECK(sx127x_lora_tx_set_for_transmission(data, data_length, device));
  int result = sx127x_set_opmod(SX127x_MODE_TX, SX127x_MODULATION_LORA, device);
  if (result == SX127X_OK) {
    ESP_LOGI(TAG, "transmitting %zu bytes on %" PRIu64, data_length, request->freq);
  }
  return result;
}

esp_err_t lora_util_read_frame(sx127x *device, uint8_t *data, uint16_t data_length, lora_frame_t **frame) {
  lora_frame_t *result = malloc(sizeof(lora_frame_t));
  if (frame == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->data_length = data_length;
  result->data = malloc(sizeof(uint8_t) * result->data_length);
  if (result->data == NULL) {
    lora_util_frame_destroy(result);
    return ESP_ERR_NO_MEM;
  }
  memcpy(result->data, data, sizeof(uint8_t) * result->data_length);
  int32_t frequency_error;
  esp_err_t code = sx127x_rx_get_frequency_error(device, &frequency_error);
  if (code == ESP_OK) {
    result->frequency_error = frequency_error;
  } else {
    ESP_LOGE(TAG, "unable to get frequency error: %s", esp_err_to_name(code));
    result->frequency_error = 0;
  }
  int16_t rssi;
  code = sx127x_rx_get_packet_rssi(device, &rssi);
  if (code == ESP_OK) {
    result->rssi = rssi;
  } else {
    ESP_LOGE(TAG, "unable to get rssi: %s", esp_err_to_name(code));
    // No room for proper "NULL". -255 should look suspicious
    result->rssi = -255;
  }
  float snr;
  code = sx127x_lora_rx_get_packet_snr(device, &snr);
  if (code == ESP_OK) {
    result->snr = snr;
  } else {
    ESP_LOGE(TAG, "unable to get snr: %s", esp_err_to_name(code));
    result->snr = -255;
  }
  struct timeval tm_vl;
  gettimeofday(&tm_vl, NULL);
  uint64_t now_micros = tm_vl.tv_sec * 1000000 + tm_vl.tv_usec;
  result->timestamp = now_micros / 1000;
  *frame = result;
  return ESP_OK;
}

uint64_t lora_util_get_min_frequency() {
  return CONFIG_MIN_FREQUENCY;
}

uint64_t lora_util_get_max_frequency() {
  return CONFIG_MAX_FREQUENCY;
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