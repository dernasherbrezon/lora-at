#include <Arduino.h>
#include <AtHandler.h>
#include <DeepSleepHandler.h>
#include <LoRaShadowClient.h>
#include <esp32-hal-log.h>
#include <esp_timer.h>
#include <lora_util.h>
#include <soc/rtc.h>
#include <sys/time.h>
extern "C" {
#include <esp_clk.h>
}

#include "BatteryVoltage.h"
#include "Display.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0"
#endif

#ifndef ARDUINO_VARIANT
#define ARDUINO_VARIANT "native"
#endif

#ifndef PIN_DI0
#define PIN_DI0 26
#endif

sx127x *device = NULL;
AtHandler *handler = NULL;
DeepSleepHandler *dsHandler = NULL;
TaskHandle_t handle_interrupt;

RTC_DATA_ATTR uint64_t rx_start_micros;
RTC_DATA_ATTR uint64_t rx_start_utc_millis;
RTC_DATA_ATTR uint64_t rx_length_micros;
RTC_DATA_ATTR uint64_t deepSleepPeriodMicros;
RTC_DATA_ATTR uint64_t inactivityTimeoutMicros;

void scheduleObservation() {
  rx_request req;
  // always attempt to load fresh config
  LoRaShadowClient *client = new LoRaShadowClient();
  client->loadRequest(&req);
  uint8_t batteryVoltage;
  int status = readVoltage(&batteryVoltage);
  if (status == 0) {
    client->sendBatteryLevel(batteryVoltage);
  }
  if (req.startTimeMillis != 0) {
    if (req.currentTimeMillis > req.endTimeMillis || req.startTimeMillis > req.endTimeMillis) {
      log_i("incorrect schedule returned from server");
      dsHandler->enterDeepSleep(0);
    } else if (req.startTimeMillis > req.currentTimeMillis) {
      dsHandler->enterDeepSleep((req.startTimeMillis - req.currentTimeMillis) * 1000);
    } else {
      // use server-side millis
      rx_length_micros = (req.endTimeMillis - req.currentTimeMillis) * 1000;
      rx_start_utc_millis = req.currentTimeMillis;
      esp_err_t rx_code = lora_util_start_rx(&req, device);
      if (rx_code != ESP_OK) {
        log_e("unable to start rx: %d", rx_code);
        return;
      }
      rx_start_micros = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
      dsHandler->enterRxDeepSleep(rx_length_micros);
    }
  } else {
    dsHandler->enterDeepSleep(0);
  }
}

void tx_callback(sx127x *callback_device) {
  log_i("transmitted");
  handler->setTransmitting(false);
}

void rx_callback_deep_sleep(sx127x *device, uint8_t *data, uint16_t data_length) {
  uint64_t now_micros = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
  uint64_t in_rx_micros = now_micros - rx_start_micros;
  uint64_t remaining_micros = rx_length_micros - in_rx_micros;

  lora_frame *frame = NULL;
  esp_err_t code = lora_util_read_frame(device, data, data_length, &frame);
  if (code != ESP_OK) {
    log_e("unable to read frame: %d", code);
    dsHandler->enterRxDeepSleep(remaining_micros);
    return;
  }
  if (frame == NULL) {
    log_i("frame wasn't received");
    dsHandler->enterRxDeepSleep(remaining_micros);
    return;
  }
  // calculate current utc millis without calling server via Bluetooth
  // this timestamp is not precise, but for short rx requests should suffice
  frame->timestamp = rx_start_utc_millis + in_rx_micros / 1000;
  log_i("frame received: %d bytes RSSI: %d SNR: %f frequency error: %d timestamp: %" PRIu64, frame->data_length, frame->rssi, frame->snr, frame->frequency_error, frame->timestamp);

  LoRaShadowClient *client = new LoRaShadowClient();
  client->sendData(frame);

  dsHandler->enterRxDeepSleep(remaining_micros);
}

void rx_callback(sx127x *device, uint8_t *data, uint16_t data_length) {
  lora_frame *frame = NULL;
  esp_err_t code = lora_util_read_frame(device, data, data_length, &frame);
  if (code != ESP_OK) {
    log_e("unable to read frame: %d", code);
    return;
  }
  if (frame == NULL) {
    log_i("frame wasn't received");
    return;
  }
  // assume time is configured using AT+TIME command
  time_t now;
  time(&now);
  frame->timestamp = now;
  log_i("frame received: %d bytes RSSI: %d SNR: %f frequency error: %d timestamp: %" PRIu64, frame->data_length, frame->rssi, frame->snr, frame->frequency_error, frame->timestamp);
  handler->addFrame(frame);
}

void IRAM_ATTR handle_interrupt_fromisr(void *arg) {
  xTaskResumeFromISR(handle_interrupt);
}

void handle_interrupt_task(void *arg) {
  while (1) {
    vTaskSuspend(NULL);
    sx127x_handle_interrupt((sx127x *)arg);
  }
}

void setup() {
  Serial.begin(115200);
  log_i("starting. firmware version: %s", FIRMWARE_VERSION);

  esp_err_t code = lora_util_init(&device);
  if (code != ESP_OK) {
    log_e("unable to start lora: %d", code);
  }

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    dsHandler = new DeepSleepHandler(deepSleepPeriodMicros, inactivityTimeoutMicros);
    code = sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, device);
    if (code != ESP_OK) {
      log_e("unable to stop lora: %d", code);
    }
    scheduleObservation();
    return;
  }
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    dsHandler = new DeepSleepHandler(deepSleepPeriodMicros, inactivityTimeoutMicros);
    sx127x_rx_set_callback(rx_callback_deep_sleep, device);
    sx127x_handle_interrupt(device);
    return;
  }

  dsHandler = new DeepSleepHandler();
  //FIXME won't be loaded when configured using AT commands
  if (dsHandler->deepSleepPeriodMicros != 0) {
    deepSleepPeriodMicros = dsHandler->deepSleepPeriodMicros;
  }
  if (dsHandler->inactivityTimeoutMicros != 0) {
    inactivityTimeoutMicros = dsHandler->inactivityTimeoutMicros;
  }

  LoRaShadowClient *client = new LoRaShadowClient();
  Display *display = new Display();
  handler = new AtHandler(device, display, client, dsHandler);

  // normal start
  sx127x_rx_set_callback(rx_callback, device);
  sx127x_tx_set_callback(tx_callback, device);
  BaseType_t task_code = xTaskCreatePinnedToCore(handle_interrupt_task, "handle interrupt", 8196, device, 2, &handle_interrupt, xPortGetCoreID());
  if (task_code != pdPASS) {
    log_e("can't create task %d", task_code);
  }

  gpio_set_direction((gpio_num_t)PIN_DI0, GPIO_MODE_INPUT);
  gpio_pulldown_en((gpio_num_t)PIN_DI0);
  gpio_pullup_dis((gpio_num_t)PIN_DI0);
  gpio_set_intr_type((gpio_num_t)PIN_DI0, GPIO_INTR_POSEDGE);
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)PIN_DI0, handle_interrupt_fromisr, (void *)device);

  display->init();
  display->setStatus("IDLE");
  display->update();
  log_i("setup completed");
}

void loop() {
  bool someActivityHappened = handler->handle(&Serial, &Serial);
  if (dsHandler->isDeepSleepRequired(someActivityHappened || handler->isReceiving())) {
    scheduleObservation();
  }
}