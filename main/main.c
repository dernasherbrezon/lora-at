#include <esp_log.h>
#include <sx127x_util.h>
#include <display.h>
#include <at_config.h>
#include <at_handler.h>
#include <ble_client.h>
#include <string.h>
#include "uart_at.h"
#include <deep_sleep.h>
#include <esp_sleep.h>

static const char *TAG = "lora-at";

#define ERROR_CHECK(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      ESP_LOGE(TAG, "unable to initialize %s: %s", y, esp_err_to_name(__err_rc));                        \
      return;        \
    }                         \
  } while (0)

typedef struct {
  sx127x *device;
  lora_at_display *display;
  at_handler_t *at_handler;
  uart_at_handler_t *uart_at_handler;
  ble_client *bluetooth;
  lora_at_config_t *config;
} main_t;

main_t *lora_at_main = NULL;

RTC_DATA_ATTR uint64_t rx_start_micros;
RTC_DATA_ATTR uint64_t rx_start_utc_millis;
RTC_DATA_ATTR uint64_t rx_length_micros;

static void uart_rx_task(void *arg) {
  main_t *main = (main_t *) arg;
  uart_at_handler_process(main->uart_at_handler);
}

static void rx_callback_deep_sleep(sx127x *device, uint8_t *data, uint16_t data_length) {
  //FIXME
  uint64_t now_micros = 0ULL;//rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
  uint64_t in_rx_micros = now_micros - rx_start_micros;
  uint64_t remaining_micros = rx_length_micros - in_rx_micros;
  lora_frame_t *frame = NULL;
  esp_err_t code = lora_util_read_frame(device, data, data_length, &frame);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to read frame: %s", esp_err_to_name(code));
    //enter deep sleep
    deep_sleep_rx_enter(remaining_micros);
    return;
  }
  // calculate current utc millis without calling server via Bluetooth
  // this timestamp is not precise, but for short rx requests should suffice
  frame->timestamp = rx_start_utc_millis + in_rx_micros / 1000;
  ESP_LOGI(TAG, "received frame: %d rssi: %d snr: %f freq_error: %" PRId32, data_length, frame->rssi, frame->snr, frame->frequency_error);
  //do not store received frames in RTC memory.
  //currently only push via bluetooth is supported
  if (lora_at_main->config->bt_address != NULL) {
    code = ble_client_send_frame(frame, lora_at_main->bluetooth);
    if (code != ESP_OK) {
      ESP_LOGE(TAG, "unable to send frame: %s", esp_err_to_name(code));
    }
    lora_util_frame_destroy(frame);
  }
  deep_sleep_rx_enter(remaining_micros);
}

static void rx_callback(sx127x *device, uint8_t *data, uint16_t data_length) {
  lora_frame_t *frame = NULL;
  ERROR_CHECK("lora frame", lora_util_read_frame(device, data, data_length, &frame));
  ESP_LOGI(TAG, "received frame: %d rssi: %d snr: %f freq_error: %" PRId32, data_length, frame->rssi, frame->snr, frame->frequency_error);
  if (lora_at_main->config->bt_address != NULL) {
    esp_err_t code = ble_client_send_frame(frame, lora_at_main->bluetooth);
    if (code != ESP_OK) {
      ESP_LOGE(TAG, "unable to send frame: %s", esp_err_to_name(code));
    }
    lora_util_frame_destroy(frame);
  } else {
    ERROR_CHECK("lora frame", at_handler_add_frame(frame, lora_at_main->at_handler));
  }
}

void tx_callback(sx127x *device) {
  const char *output = "OK\r\n";
  uart_at_handler_send((char *) output, strlen(output), lora_at_main->uart_at_handler);
  lora_at_display_set_status("IDLE", lora_at_main->display);
}

void schedule_observation() {
  rx_request_t *req = NULL;
  esp_err_t code = ble_client_load_request(&req, lora_at_main->bluetooth);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to read rx request: %s", esp_err_to_name(code));
    deep_sleep_enter(lora_at_main->config->deep_sleep_period_micros);
    return;
  }
  if (req == NULL) {
    ESP_LOGI(TAG, "no active requests");
    deep_sleep_enter(lora_at_main->config->deep_sleep_period_micros);
    return;
  }
  if (req->currentTimeMillis > req->endTimeMillis || req->startTimeMillis > req->endTimeMillis) {
    ESP_LOGE(TAG, "incorrect schedule found on server current: %" PRIu64 " start: %" PRIu64 " end: %" PRIu64, req->currentTimeMillis, req->startTimeMillis, req->endTimeMillis);
    deep_sleep_enter(lora_at_main->config->deep_sleep_period_micros);
    return;
  }
  if (req->startTimeMillis > req->currentTimeMillis) {
    deep_sleep_enter((req->startTimeMillis - req->currentTimeMillis) * 1000);
    return;
  }
  // observation is actually should start now
  rx_length_micros = (req->endTimeMillis - req->currentTimeMillis) * 1000;
  rx_start_utc_millis = req->currentTimeMillis;
  code = lora_util_start_rx(req, lora_at_main->device);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "cannot start rx: %s", esp_err_to_name(code));
    deep_sleep_enter(lora_at_main->config->deep_sleep_period_micros);
    return;
  }
  //FIXME
//  rx_start_micros = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
  deep_sleep_rx_enter(rx_length_micros);
}

void app_main(void) {
  lora_at_main = malloc(sizeof(main_t));
  if (lora_at_main == NULL) {
    ESP_LOGE(TAG, "unable to init main");
    return;
  }

  ERROR_CHECK("config", lora_at_config_create(&lora_at_main->config));
  ESP_LOGI(TAG, "config initialized");

  ERROR_CHECK("bluetooth", ble_client_create(&lora_at_main->bluetooth));
  if (lora_at_main->config->bt_address != NULL) {
    uint8_t val[6];
    ERROR_CHECK("unable to convert address to hex", at_util_string2hex_allocated(lora_at_main->config->bt_address, val));
    ERROR_CHECK("bluetooth", ble_client_connect(val, lora_at_main->bluetooth));
    ESP_LOGI(TAG, "bluetooth initialized: %s", lora_at_main->config->bt_address);
  } else {
    ESP_LOGI(TAG, "bluetooth not initialized");
  }

  ERROR_CHECK("lora", lora_util_init(&lora_at_main->device));
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    ESP_LOGI(TAG, "woken up by timer. loading new rx request");
    esp_err_t code = sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, lora_at_main->device);
    if (code != ESP_OK) {
      ESP_LOGE(TAG, "unable to put sx127x to sleep: %s", esp_err_to_name(code));
    }
    schedule_observation(); // should always put esp32 into deep sleep. so can return from here
    return;
  }
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    ESP_LOGI(TAG, "woken up by incoming message. loading the message");
    sx127x_rx_set_callback(rx_callback_deep_sleep, lora_at_main->device);
    sx127x_handle_interrupt(lora_at_main->device); // should always put esp32 into deep sleep. so can return from here
    return;
  }
  sx127x_rx_set_callback(rx_callback, lora_at_main->device);
  sx127x_tx_set_callback(tx_callback, lora_at_main->device);
  ESP_LOGI(TAG, "sx127x initialized");

  ERROR_CHECK("display", lora_at_display_create(&lora_at_main->display));
  lora_at_display_set_status("IDLE", lora_at_main->display);
  if (lora_at_main->config->init_display) {
    ERROR_CHECK("display", lora_at_display_start(lora_at_main->display));
    ESP_LOGI(TAG, "display started");
  } else {
    ESP_LOGI(TAG, "display NOT started");
  }

  ERROR_CHECK("at_handler", at_handler_create(lora_at_main->config, lora_at_main->display, lora_at_main->device, lora_at_main->bluetooth, &lora_at_main->at_handler));
  ESP_LOGI(TAG, "at handler initialized");

  ERROR_CHECK("uart_at", uart_at_handler_create(lora_at_main->at_handler, &lora_at_main->uart_at_handler));
  ESP_LOGI(TAG, "uart initialized");

  xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 4, lora_at_main, configMAX_PRIORITIES, NULL);
  ESP_LOGI(TAG, "lora-at initialized");

  //FIXME implement inactivity period

}
