#include <stdio.h>
#include <esp_log.h>
#include <sx127x_util.h>
#include <display.h>
#include <at_config.h>
#include <at_handler.h>
#include <ble_client.h>
#include <string.h>
#include "uart_at.h"

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
} main_t;

main_t *lora_at_main = NULL;

static void uart_rx_task(void *arg) {
  main_t *main = (main_t *) arg;
  uart_at_handler_process(main->uart_at_handler);
}

static void rx_callback(sx127x *device, uint8_t *data, uint16_t data_length) {
  lora_frame_t *frame = NULL;
  ERROR_CHECK("lora frame", lora_util_read_frame(device, data, data_length, &frame));
  ESP_LOGI(TAG, "received frame: %d rssi: %d snr: %f freq_error: %" PRId32, data_length, frame->rssi, frame->snr, frame->frequency_error);
  //TODO check push vs pull
  ERROR_CHECK("lora frame", at_handler_add_frame(frame, lora_at_main->at_handler));
}

void tx_callback(sx127x *device) {
  const char *output = "OK\r\n";
  uart_at_handler_send((char *) output, strlen(output), lora_at_main->uart_at_handler);
  lora_at_display_set_status("IDLE", lora_at_main->display);
}

void app_main(void) {
  lora_at_main = malloc(sizeof(main_t));
  if (lora_at_main == NULL) {
    ESP_LOGE(TAG, "unable to init main");
    return;
  }

  lora_at_config_t *config = NULL;
  ERROR_CHECK("config", lora_at_config_create(&config));
  ESP_LOGI(TAG, "config initialized");
  ERROR_CHECK("display", lora_at_display_create(&lora_at_main->display));
  lora_at_display_set_status("IDLE", lora_at_main->display);
  if (config->init_display) {
    ERROR_CHECK("display", lora_at_display_start(lora_at_main->display));
    ESP_LOGI(TAG, "display started");
  } else {
    ESP_LOGI(TAG, "display NOT started");
  }

  ERROR_CHECK("lora", lora_util_init(&lora_at_main->device));
  sx127x_rx_set_callback(rx_callback, lora_at_main->device);
  sx127x_tx_set_callback(tx_callback, lora_at_main->device);
  ESP_LOGI(TAG, "lora initialized");

  ERROR_CHECK("bluetooth", ble_client_create(&lora_at_main->bluetooth));
  ESP_LOGI(TAG, "bluetooth initialized");

  ERROR_CHECK("at_handler", at_handler_create(config, lora_at_main->display, lora_at_main->device, lora_at_main->bluetooth, &lora_at_main->at_handler));
  ESP_LOGI(TAG, "at handler initialized");

  ERROR_CHECK("uart_at", uart_at_handler_create(lora_at_main->at_handler, &lora_at_main->uart_at_handler));
  ESP_LOGI(TAG, "uart initialized");


  xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 4, lora_at_main, configMAX_PRIORITIES, NULL);
  ESP_LOGI(TAG, "lora-at initialized");


//  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
//  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
//    ds_handler = new    DeepSleepHandler(deepSleepPeriodMicros, inactivityTimeoutMicros);
//    code = sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, device);
//    if (code != ESP_OK) {
//      log_e("unable to stop lora: %d", code);
//    }
//    scheduleObservation();
//    return;
//  }
//  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
//    ds_handler = new
//    DeepSleepHandler(deepSleepPeriodMicros, inactivityTimeoutMicros);
//    sx127x_rx_set_callback(rx_callback_deep_sleep, device);
//    sx127x_handle_interrupt(device);
//    return;
//  }
//
//  ds_handler = new
//  DeepSleepHandler();
//  //FIXME won't be loaded when configured using AT commands
//  if (ds_handler->deepSleepPeriodMicros != 0) {
//    deepSleepPeriodMicros = ds_handler->deepSleepPeriodMicros;
//  }
//  if (ds_handler->inactivityTimeoutMicros != 0) {
//    inactivityTimeoutMicros = ds_handler->inactivityTimeoutMicros;
//  }
//
//  LoRaShadowClient *client = new
//  LoRaShadowClient();
//  Display *display = new
//  Display();
//  handler = new
//  AtHandler(device, display, client, ds_handler);
//
//  // normal start
//  sx127x_rx_set_callback(rx_callback, device);
//  sx127x_tx_set_callback(tx_callback, device);
//  BaseType_t task_code = xTaskCreatePinnedToCore(handle_interrupt_task, "handle interrupt", 8196, device, 2, &handle_interrupt, xPortGetCoreID());
//  if (task_code != pdPASS) {
//    log_e("can't create task %d", task_code);
//  }
//
//  gpio_set_direction((gpio_num_t) PIN_DI0, GPIO_MODE_INPUT);
//  gpio_pulldown_en((gpio_num_t) PIN_DI0);
//  gpio_pullup_dis((gpio_num_t) PIN_DI0);
//  gpio_set_intr_type((gpio_num_t) PIN_DI0, GPIO_INTR_POSEDGE);
//  gpio_install_isr_service(0);
//  gpio_isr_handler_add((gpio_num_t) PIN_DI0, handle_interrupt_fromisr, (void *) device);
//
//  display->init();
//  display->setStatus("IDLE");
//  display->update();
//  log_i("setup completed");
}