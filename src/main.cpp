#include <Arduino.h>
#include <AtHandler.h>
#include <DeepSleepHandler.h>
#include <LoRaModule.h>
#include <LoRaShadowClient.h>
#include <esp32-hal-log.h>
#include <esp_timer.h>
#include <time.h>

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
  if (scheduledObservation.startTimeMillis != 0) {
    if (scheduledObservation.startTimeMillis > scheduledObservation.currentTimeMillis) {
      dsHandler->enterDeepSleep((scheduledObservation.startTimeMillis - scheduledObservation.currentTimeMillis) * 1000);
    } else {
      // do not setup/mess with local timer during deep sleep mode
      // use server-side millis
      stopObservationMicros = esp_timer_get_time() + (scheduledObservation.endTimeMillis - scheduledObservation.currentTimeMillis) * 1000;
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

  dsHandler = new DeepSleepHandler();
  if (dsHandler->isDeepSleepWakeup()) {
    scheduleObservation();
  }

  display = new Display();
  display->init();

  handler = new AtHandler(lora, display, client, dsHandler);
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
    return;
  }

  // FIXME thread sleep 1sec? Or some task based thing?

  if (stopObservationMicros != 0 && stopObservationMicros < esp_timer_get_time()) {
    lora->stopRx();
    scheduleObservation();
    return;
  }

  bool someActivityHappened = handler->handle(&Serial, &Serial);
  dsHandler->handleInactive(someActivityHappened);
}