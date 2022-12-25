#include <Arduino.h>
#include <AtHandler.h>
#include <DeepSleepHandler.h>
#include <LoRaModule.h>
#include <LoRaShadowClient.h>
#include <esp32-hal-log.h>
#include <esp_timer.h>
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

LoRaModule *lora = NULL;
AtHandler *handler;
Display *display = NULL;
LoRaShadowClient *client = NULL;
DeepSleepHandler *dsHandler = NULL;

RTC_DATA_ATTR uint64_t sleepTime;
RTC_DATA_ATTR uint64_t observation_length_micros;
uint64_t stopObservationMicros = 0;

void scheduleObservation() {
  ObservationRequest scheduledObservation;
  // always attempt to load fresh config
  client->loadRequest(&scheduledObservation);
  uint8_t batteryVoltage;
  int status = readVoltage(&batteryVoltage);
  if (status == 0) {
    client->sendBatteryLevel(batteryVoltage);
  }
  if (scheduledObservation.startTimeMillis != 0) {
    if (scheduledObservation.currentTimeMillis > scheduledObservation.endTimeMillis || scheduledObservation.startTimeMillis > scheduledObservation.endTimeMillis) {
      log_i("incorrect schedule returned from server");
      dsHandler->enterDeepSleep(0);
    } else if (scheduledObservation.startTimeMillis > scheduledObservation.currentTimeMillis) {
      dsHandler->enterDeepSleep((scheduledObservation.startTimeMillis - scheduledObservation.currentTimeMillis) * 1000);
    } else {
      // use server-side millis
      observation_length_micros = (scheduledObservation.endTimeMillis - scheduledObservation.currentTimeMillis) * 1000;
      stopObservationMicros = esp_timer_get_time() + observation_length_micros;
      // set current server time
      // LoRaModule will use current time for just received frame
      struct timeval now;
      now.tv_sec = scheduledObservation.currentTimeMillis / 1000;
      now.tv_usec = 0;
      int code = settimeofday(&now, NULL);
      if (code != 0) {
        log_e("unable to set current time. LoRa frame timestamp will be incorrect");
      }
      lora->startLoraRx(&scheduledObservation);
      sleepTime = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
      dsHandler->enterRxDeepSleep(observation_length_micros);
    }
  } else {
    dsHandler->enterDeepSleep(0);
  }
}

void handle_packet() {
  if (lora == NULL || lora->device == NULL) {
    return;
  }
  uint8_t *data = NULL;
  uint8_t data_length = 0;
  esp_err_t code = sx1278_receive(lora->device, &data, &data_length);
  if (code != ESP_OK) {
    Serial.printf("can't read %d", code);
    return;
  }
  if (data_length == 0) {
    // no message received
    return;
  }
  LoRaFrame *result = (LoRaFrame *)malloc(sizeof(LoRaFrame_t));
  if (result == NULL) {
    return;
  }
  result->dataLength = data_length;
  result->data = (uint8_t *)malloc(sizeof(uint8_t) * result->dataLength);
  if (result->data == NULL) {
    LoRaFrame_destroy(result);
    return;
  }

  memcpy(result->data, data, result->dataLength);
  time_t now;
  time(&now);
  result->timestamp = now;
  int16_t rssi = 0;
  sx1278_get_packet_rssi(lora->device, &rssi);
  result->rssi = rssi;
  sx1278_get_packet_snr(lora->device, &result->snr);
  int32_t frequency_error = 0;
  sx1278_get_frequency_error(lora->device, &frequency_error);
  result->frequencyError = frequency_error;
  log_i("frame received: %d bytes RSSI: %f SNR: %f Timestamp: %ld", result->dataLength, result->rssi, result->snr, result->timestamp);

  client->sendData(result);
  handler->addFrame(result);
}

void setup() {
  Serial.begin(115200);
  log_i("starting. firmware version: %s", FIRMWARE_VERSION);

  lora = new LoRaModule();

  client = new LoRaShadowClient();
  display = new Display();
  dsHandler = new DeepSleepHandler();
  handler = new AtHandler(lora, display, client, dsHandler);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    scheduleObservation();
  } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    sx1278_handle_interrupt(lora->device);
    handle_packet();
    uint64_t timeNow = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
    uint64_t remaining_micros = observation_length_micros - (timeNow - sleepTime);
    dsHandler->enterRxDeepSleep(remaining_micros);
  }

  display->init();

  display->setStatus("IDLE");
  display->update();
  log_i("setup completed");
}

void loop() {
  if (stopObservationMicros != 0 && stopObservationMicros < esp_timer_get_time()) {
    lora->stopRx();
    // read next observation and go to deep sleep
    scheduleObservation();
    return;
  }

  bool someActivityHappened = handler->handle(&Serial, &Serial);
  dsHandler->handleInactive(someActivityHappened || lora->isReceivingData());
}