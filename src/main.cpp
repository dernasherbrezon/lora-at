#include <AtHandler.h>
#include <LoRaModule.h>
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

bool chipInitializationFailed = false;

const char *getStatus() {
  // SETUP - waiting for AP to initialize
  // CONNECTING - connecting to WiFi
  // RECEIVING - lora is listening for data
  // IDLE - module is waiting for rx/tx requests
  // CHIP FAIL - chip initialization failed
  if (chipInitializationFailed) {
    return "CHIP FAIL";
  }
  if (lora != NULL && lora->isReceivingData()) {
    return "RECEIVING";
  } else {
    return "IDLE";
  }
}

void handleStatus() {
  // if (!web->authenticate(conf->getUsername(), conf->getPassword())) {
  //   web->requestAuthentication();
  //   return;
  // }
  // StaticJsonDocument<256> json;
  // json["status"] = getStatus();
  // int8_t sxTemperature;
  // if (lora->getTempRaw(&sxTemperature) == 0) {
  //   json["chipTemperature"] = sxTemperature;
  // }

  // Chip *chip = conf->getChip();
  // if (chip != NULL) {
  //   if (chip->loraSupported) {
  //     JsonObject obj = json.createNestedObject("lora");
  //     obj["minFreq"] = chip->minLoraFrequency;
  //     obj["maxFreq"] = chip->maxLoraFrequency;
  //   }
  //   if (chip->fskSupported) {
  //     JsonObject obj = json.createNestedObject("fsk");
  //     obj["minFreq"] = chip->minFskFrequency;
  //     obj["maxFreq"] = chip->maxFskFrequency;
  //   }
  // }

  // String output;
  // serializeJson(json, output);
  // web->send(200, "application/json; charset=UTF-8", output);
}

void setup() {
  Serial.begin(115200);
  log_i("starting. firmware version: %s", FIRMWARE_VERSION);

  display = new Display();
  display->init();

  lora = new LoRaModule();
  lora->setOnRxStartedCallback([] {
    display->setStatus(getStatus());
    display->update();
  });
  lora->setOnRxStoppedCallback([] {
    display->setStatus(getStatus());
    display->update();
  });

  Serial.readString();

  handler = new AtHandler(lora);

  // web->on("/status", HTTP_GET, []() {
  //   handleStatus();
  // });

  display->setStatus(getStatus());
  display->update();
  log_i("setup completed");
}

void loop() {
  handler->handle(&Serial, &Serial);
}