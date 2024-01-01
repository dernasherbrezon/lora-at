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
#include <at_timer.h>
#include <sys/time.h>
#include <sdkconfig.h>
#include <driver/gpio.h>
#include <at_sensors.h>
#include <at_wifi.h>
#include <at_rest.h>

static const char *TAG = "lora-at";

#ifndef CONFIG_SX127X_POWER_PROFILING
#define CONFIG_SX127X_POWER_PROFILING -1
#endif

#ifndef CONFIG_BLUETOOTH_RECONNECTION_INTERVAL
#define CONFIG_BLUETOOTH_RECONNECTION_INTERVAL 5000
#endif

#define ERROR_CHECK(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      ESP_LOGE(TAG, "unable to initialize %s: %s", y, esp_err_to_name(__err_rc));                        \
      return;        \
    }                         \
  } while (0)

#define ERROR_CHECK_DS(y, x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      ESP_LOGE(TAG, "unable to initialize %s: %s", y, esp_err_to_name(__err_rc)); \
      deep_sleep_enter(CONFIG_BLUETOOTH_RECONNECTION_INTERVAL * 1000);                              \
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
  at_timer_t *timer;
  at_rest *rest;
  int cad_mode;
} main_t;

main_t *lora_at_main = NULL;

RTC_DATA_ATTR uint64_t rx_end_micros;

static void uart_rx_task(void *arg) {
  main_t *main = (main_t *) arg;
  uart_at_handler_process(main->uart_at_handler);
}

void main_deep_sleep_enter(uint64_t remaining_micros) {
  sx127x_util_deep_sleep_enter(lora_at_main->device);
  lora_at_display_deep_sleep_enter();
  deep_sleep_enter(remaining_micros);
}

static void rx_callback_deep_sleep(sx127x *device, uint8_t *data, uint16_t data_length) {
  struct timeval tm_vl;
  gettimeofday(&tm_vl, NULL);
  uint64_t now_micros = tm_vl.tv_sec * 1000000 + tm_vl.tv_usec;
  uint64_t remaining_micros;
  if (rx_end_micros > now_micros) {
    // if we received the message just before the end
    // wake up took some time so current time can be slightly more than expected end
    remaining_micros = rx_end_micros - now_micros;
  } else {
    remaining_micros = 0;
  }
  lora_frame_t *frame = NULL;
  esp_err_t code = sx127x_util_read_frame(device, data, data_length, SX127x_MODULATION_LORA, &frame);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to read frame: %s", esp_err_to_name(code));
    //enter deep sleep
    deep_sleep_rx_enter(remaining_micros);
    return;
  }
  ESP_LOGI(TAG, "received frame: %d rssi: %d snr: %f freq_error: %" PRId32, data_length, frame->rssi, frame->snr, frame->frequency_error);
  //do not store received frames in RTC memory.
  //currently only push via bluetooth is supported
  if (lora_at_main->config->bt_address != NULL) {
    code = ble_client_send_frame(frame, lora_at_main->bluetooth);
    if (code != ESP_OK) {
      ESP_LOGE(TAG, "unable to send frame: %s", esp_err_to_name(code));
    }
    sx127x_util_frame_destroy(frame);
  }
  deep_sleep_rx_enter(remaining_micros);
}

static void rx_callback(sx127x *device, uint8_t *data, uint16_t data_length) {
  lora_frame_t *frame = NULL;
  ERROR_CHECK("lora frame", sx127x_util_read_frame(device, data, data_length, lora_at_main->at_handler->active_mode, &frame));
  ESP_LOGI(TAG, "received frame: %d rssi: %d snr: %f freq_error: %" PRId32, data_length, frame->rssi, frame->snr, frame->frequency_error);
  if (lora_at_main->config->bt_address != NULL) {
    esp_err_t code = ble_client_send_frame(frame, lora_at_main->bluetooth);
    if (code != ESP_OK) {
      ESP_LOGE(TAG, "unable to send frame: %s", esp_err_to_name(code));
    }
    sx127x_util_frame_destroy(frame);
  } else {
#if CONFIG_AT_WIFI_ENABLED
    ERROR_CHECK("lora frame", at_rest_add_frame(frame, lora_at_main->rest));
#else
    ERROR_CHECK("lora frame", at_handler_add_frame(frame, lora_at_main->at_handler));
#endif
  }
  // this rx message was received using CAD<->RX mode
  // put back into CAD mode
  if (lora_at_main->cad_mode == 1) {
    ERROR_CHECK("cad mode", sx127x_set_opmod(SX127x_MODE_CAD, SX127x_MODULATION_LORA, device));
  }
}

void tx_callback(sx127x *device) {
  if (CONFIG_SX127X_POWER_PROFILING > 0) {
    gpio_set_level((gpio_num_t) CONFIG_SX127X_POWER_PROFILING, 0);
  }
  const char *output = "OK\r\n";
  uart_at_handler_send((char *) output, strlen(output), lora_at_main->uart_at_handler);
  lora_at_display_set_status("IDLE", lora_at_main->display);
}

void cad_callback(sx127x *device, int cad_detected) {
  if (cad_detected == 0) {
    ESP_LOGD(TAG, "cad not detected");
    lora_at_main->cad_mode = 0;
    ERROR_CHECK("cad mode", sx127x_set_opmod(SX127x_MODE_CAD, SX127x_MODULATION_LORA, device));
    return;
  }
  // put into RX mode first to handle interrupt as soon as possible
  lora_at_main->cad_mode = 1;
  ERROR_CHECK("rx single", sx127x_set_opmod(SX127x_MODE_RX_SINGLE, SX127x_MODULATION_LORA, device));
  ESP_LOGD(TAG, "cad detected");
}

void send_status(main_t *main) {
  ble_client_status status;
  ERROR_CHECK("sensors", at_sensors_init());
  ERROR_CHECK("solar", at_sensors_get_solar(&status.solar_voltage, &status.solar_current));
  ERROR_CHECK("battery", at_sensors_get_battery(&status.battery_voltage, &status.battery_current));
  at_sensors_destroy();
  ERROR_CHECK("sx127x temperature", sx127x_util_read_temperature(main->device, &(status.sx127x_raw_temperature)));
  ERROR_CHECK("bluetooth rssi", ble_client_get_rssi(main->bluetooth, &(status.rssi)));
  ERROR_CHECK("send status", ble_client_send_status(&status, main->bluetooth));
}

void schedule_observation_and_go_ds(main_t *main) {
  lora_config_t *req = NULL;
  if (main->config->bt_address != NULL) {
    send_status(main);
    ERROR_CHECK_DS("rx request", ble_client_load_request(&req, main->bluetooth));
  }
  if (req == NULL) {
    ESP_LOGI(TAG, "no active requests");
    main_deep_sleep_enter(main->config->deep_sleep_period_micros);
    return;
  }
  if (req->currentTimeMillis > req->endTimeMillis || req->startTimeMillis > req->endTimeMillis) {
    ESP_LOGE(TAG, "incorrect schedule found on server current: %" PRIu64 " start: %" PRIu64 " end: %" PRIu64, req->currentTimeMillis, req->startTimeMillis, req->endTimeMillis);
    main_deep_sleep_enter(main->config->deep_sleep_period_micros);
    return;
  }
  if (req->startTimeMillis > req->currentTimeMillis) {
    main_deep_sleep_enter((req->startTimeMillis - req->currentTimeMillis) * 1000);
    return;
  }
  // set time before doing rx
  // time will be used in rx callback to figure out how long to sleep
  struct timeval tm_vl;
  tm_vl.tv_sec = req->currentTimeMillis / 1000;
  tm_vl.tv_usec = 0;
  settimeofday(&tm_vl, NULL);
  // observation actually should start now
  ERROR_CHECK_DS("start rx", sx127x_util_lora_rx(SX127x_MODE_RX_CONT, req, main->device));
  rx_end_micros = req->endTimeMillis * 1000;
  deep_sleep_rx_enter((req->endTimeMillis - req->currentTimeMillis) * 1000);
}

static void at_timer_callback(void *arg) {
  main_t *main = (main_t *) arg;
  uint64_t last_active = 0;
  uart_at_get_last_active(&last_active, main->uart_at_handler);
  uint64_t current_counter = 0;
  at_timer_get_counter(&current_counter, main->timer);
  uint64_t remaining = current_counter - last_active;
  if (remaining > main->config->inactivity_period_micros) {
    ESP_LOGI(TAG, "inactive for %.2f seconds", (remaining / 1000000.0F));
    schedule_observation_and_go_ds(main);
  } else {
    at_timer_set_counter(remaining, main->timer);
  }
}

void app_main(void) {
  lora_at_main = malloc(sizeof(main_t));
  if (lora_at_main == NULL) {
    ESP_LOGE(TAG, "unable to init main");
    return;
  }
  lora_at_main->cad_mode = 0;
  lora_at_main->device = NULL;

  ERROR_CHECK("config", lora_at_config_create(&lora_at_main->config));
  ESP_LOGI(TAG, "config initialized");

  ERROR_CHECK("bluetooth", ble_client_create(lora_at_main->config->bt_address, &lora_at_main->bluetooth));
  if (lora_at_main->config->bt_address != NULL) {
    ESP_LOGI(TAG, "bluetooth initialized");
  } else {
    ESP_LOGI(TAG, "bluetooth not initialized");
  }

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    ESP_LOGI(TAG, "woken up by timer. loading new rx request");
    ERROR_CHECK("lora", sx127x_util_init(&lora_at_main->device));
    esp_err_t code = sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, lora_at_main->device);
    if (code != ESP_OK) {
      ESP_LOGE(TAG, "unable to put sx127x to sleep: %s", esp_err_to_name(code));
    }
    schedule_observation_and_go_ds(lora_at_main); // should always put esp32 into deep sleep. so can return from here
    return;
  }
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    ESP_LOGI(TAG, "woken up by incoming message. loading the message");
    ERROR_CHECK("lora", sx127x_util_init(&lora_at_main->device));
    sx127x_rx_set_callback(rx_callback_deep_sleep, lora_at_main->device);
    sx127x_handle_interrupt(lora_at_main->device); // should always put esp32 into deep sleep. so can return from here
    return;
  }
  esp_log_level_set("*", ESP_LOG_INFO);
  // reset whatever state was before
  esp_err_t code = sx127x_util_reset();
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to reset sx127x chip");
  }
  ERROR_CHECK("lora", sx127x_util_init(&lora_at_main->device));
  ERROR_CHECK("lora sleep", sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, lora_at_main->device));
  sx127x_rx_set_callback(rx_callback, lora_at_main->device);
  sx127x_tx_set_callback(tx_callback, lora_at_main->device);
  sx127x_lora_cad_set_callback(cad_callback, lora_at_main->device);
  ESP_LOGI(TAG, "sx127x initialized");

  ERROR_CHECK("display", lora_at_display_create(&lora_at_main->display));
  lora_at_display_set_status("IDLE", lora_at_main->display);
  if (lora_at_main->config->init_display) {
    ERROR_CHECK("display", lora_at_display_start(lora_at_main->display));
    ESP_LOGI(TAG, "display started");
  } else {
    ESP_LOGI(TAG, "display NOT started");
  }

  ERROR_CHECK("timer", at_timer_create(at_timer_callback, lora_at_main, &lora_at_main->timer));
  if (lora_at_main->config->inactivity_period_micros != 0 && !CONFIG_AT_WIFI_ENABLED) {
    ERROR_CHECK("timer", at_timer_start(lora_at_main->config->inactivity_period_micros, lora_at_main->timer));
  }

  ERROR_CHECK("at_handler", at_handler_create(lora_at_main->config, lora_at_main->display, lora_at_main->device, lora_at_main->bluetooth, lora_at_main->timer, &lora_at_main->at_handler));
  ESP_LOGI(TAG, "at handler initialized");

  ERROR_CHECK("uart_at", uart_at_handler_create(lora_at_main->at_handler, lora_at_main->timer, &lora_at_main->uart_at_handler));
  ESP_LOGI(TAG, "uart initialized");
  xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 4, lora_at_main, configMAX_PRIORITIES, NULL);

  ERROR_CHECK("at_wifi", at_wifi_connect());
  ERROR_CHECK("at_rest", at_rest_create(lora_at_main->device, &lora_at_main->rest));
  ESP_LOGI(TAG, "lora-at initialized");
}
