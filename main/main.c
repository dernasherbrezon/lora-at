#include <stdio.h>
#include <esp_log.h>
#include <sx127x_util.h>
#include <display.h>
#include <at_config.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0"
#endif

static const char *TAG = "lora-at";

typedef struct {
  sx127x *device;
  lora_at_display_t *display;
} main_t;

void app_main(void) {
  ESP_LOGI(TAG, "firmware version: %s", FIRMWARE_VERSION);

  main_t *main = malloc(sizeof(main_t));
  if (main == NULL) {
    ESP_LOGE(TAG, "unable to init main");
    return;
  }

  lora_at_config_t *config = NULL;
  esp_err_t code = lora_at_config_create(&config);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to initialize config: %d", code);
    return;
  }
  ESP_LOGI(TAG, "config initialized");
  if (config->init_display) {
    code = lora_at_display_create(&main->display);
    if (code != ESP_OK) {
      ESP_LOGE(TAG, "unable to initialize display: %d", code);
      return;
    }
    ESP_LOGI(TAG, "display initialized");
    lora_at_display_set_status("IDLE", main->display);
  } else {
    ESP_LOGI(TAG, "display NOT initialized");
    main->display = NULL;
  }

  code = lora_util_init(&main->device);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to initialize lora: %d", code);
    return;
  }
  ESP_LOGI(TAG, "lora initialized");
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