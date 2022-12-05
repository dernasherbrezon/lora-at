#include <Arduino.h>
#include <AtHandler.h>
#include <DeepSleepHandler.h>
#include <LoRaModule.h>
#include <LoRaShadowClient.h>
#include <esp32-hal-log.h>
#include <esp_timer.h>
#include <sys/time.h>

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

RTC_DATA_ATTR ObservationRequest scheduledObservation;
uint64_t stopObservationMicros = 0;

void scheduleObservation() {
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
      stopObservationMicros = esp_timer_get_time() + (scheduledObservation.endTimeMillis - scheduledObservation.currentTimeMillis) * 1000;
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
    }
  } else {
    dsHandler->enterDeepSleep(0);
  }
}

void setup() {
  Serial.begin(115200);
  log_i("starting. firmware version: %s", FIRMWARE_VERSION);

  lora = new LoRaModule();
  lora->setOnRxStartedCallback([] {
    display->setStatus("RECEIVING");
    display->update();
  });
  lora->setOnRxStoppedCallback([] {
    display->setStatus("IDLE");
    display->update();
  });

  client = new LoRaShadowClient();
  display = new Display();
  dsHandler = new DeepSleepHandler();
  handler = new AtHandler(lora, display, client, dsHandler);
  
  if (dsHandler->isDeepSleepWakeup()) {
    scheduleObservation();
  }

  display->init();

  display->setStatus("IDLE");
  display->update();
  log_i("setup completed");
}

void loop() {
  if (lora->isReceivingData()) {
    LoRaFrame *frame = lora->loop();
    if (frame != NULL) {
      client->sendData(frame);
      handler->addFrame(frame);
    }
  }

  if (stopObservationMicros != 0 && stopObservationMicros < esp_timer_get_time()) {
    lora->stopRx();
    // read next observation and go to deep sleep
    scheduleObservation();
    return;
  }

  bool someActivityHappened = handler->handle(&Serial, &Serial);
  dsHandler->handleInactive(someActivityHappened || lora->isReceivingData());
}