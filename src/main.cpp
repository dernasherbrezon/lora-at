#include <Arduino.h>
#include <AtHandler.h>
#include <DeepSleepHandler.h>
#include <LoRaModule.h>
#include <LoRaShadowClient.h>
#include <esp32-hal-log.h>
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

void setup() {
  Serial.begin(115200);
  log_i("starting. firmware version: %s", FIRMWARE_VERSION);

  display = new Display();
  display->init();

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

  handler = new AtHandler(lora, display, client, dsHandler);
  display->setStatus("IDLE");
  display->update();
  log_i("setup completed");
}

void loop() {
  handler->handle(&Serial, &Serial);
  dsHandler->loop();
}