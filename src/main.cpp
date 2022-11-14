#include <Arduino.h>
#include <AtHandler.h>
#include <DeepSleepHandler.h>
#include <LoRaModule.h>
#include <LoRaShadowClient.h>
#include <esp32-hal-log.h>
#include <esp_timer.h>
#include <sys/time.h>

#include "Display.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0"
#endif

#ifndef ARDUINO_VARIANT
#define ARDUINO_VARIANT "native"
#endif

#ifndef PIN_BATTERY_VOLTAGE
#define PIN_BATTERY_VOLTAGE 0
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
  if (PIN_BATTERY_VOLTAGE != 0) {
    //TODO should it be configured? Any timeout needed?
    pinMode(PIN_BATTERY_VOLTAGE, INPUT);
    client->sendBatteryLevel(analogRead(PIN_BATTERY_VOLTAGE));
  }
  if (scheduledObservation.startTimeMillis != 0) {
    if (scheduledObservation.startTimeMillis > scheduledObservation.currentTimeMillis) {
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
      // FIXME destroy frames?
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