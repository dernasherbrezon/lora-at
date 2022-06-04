#include "Display.h"

#include <esp32-hal-log.h>

// defined in platformio.ini
#ifndef PIN_OLED_SDA
#define PIN_OLED_SDA 0
#endif
#ifndef PIN_OLED_SCL
#define PIN_OLED_SCL 0
#endif

#define logo_width 32
#define logo_height 32
static uint8_t logo_bits[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x38,
    0x00,
    0x00,
    0x00,
    0xFC,
    0x00,
    0x00,
    0x00,
    0xFE,
    0x01,
    0x00,
    0x00,
    0xFC,
    0x01,
    0x00,
    0x00,
    0xF8,
    0x07,
    0x00,
    0x00,
    0xF0,
    0x07,
    0x00,
    0x00,
    0xE0,
    0x0F,
    0x00,
    0x00,
    0xC0,
    0x3F,
    0x00,
    0x00,
    0x80,
    0x7F,
    0x00,
    0x00,
    0x00,
    0x3F,
    0x01,
    0x00,
    0x00,
    0xBE,
    0x07,
    0x00,
    0x00,
    0xDC,
    0x07,
    0x00,
    0x00,
    0xE8,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF8,
    0x0F,
    0x00,
    0x00,
    0xFE,
    0x27,
    0x00,
    0x00,
    0xFE,
    0x3B,
    0x00,
    0x00,
    0xFC,
    0xFD,
    0x00,
    0x00,
    0x7C,
    0xFE,
    0x01,
    0x00,
    0xF0,
    0xFC,
    0x01,
    0x01,
    0x41,
    0xF8,
    0x07,
    0xB2,
    0x01,
    0xF0,
    0x07,
    0x22,
    0x03,
    0xE0,
    0x1F,
    0x66,
    0x00,
    0xC0,
    0x1F,
    0xC4,
    0x01,
    0x80,
    0x3F,
    0x1C,
    0x02,
    0x00,
    0x7F,
    0x18,
    0x00,
    0x00,
    0x3E,
    0x70,
    0x00,
    0x00,
    0x1C,
    0xC0,
    0x01,
    0x00,
    0x08,
    0x00,
    0x01,
    0x00,
    0x00,
};

void Display::init() {
  if (PIN_OLED_SDA == 0 || PIN_OLED_SCL == 0) {
    log_i("OLED pins were not configured. continue without display");
    return;
  }
  this->display = new SSD1306Wire(0x3c, PIN_OLED_SDA, PIN_OLED_SCL);
#ifdef PIN_OLED_RST
  pinMode(PIN_OLED_RST, OUTPUT);
  digitalWrite(PIN_OLED_RST, LOW);
  delay(50);
  digitalWrite(PIN_OLED_RST, HIGH);
#endif
  if (!this->display->init()) {
    log_e("unable initialize display. continue without display");
    delete this->display;
    this->display = NULL;
    return;
  }
  log_i("display was initialized");
}

void Display::update() {
  if (this->display == NULL || !this->enabled) {
    return;
  }
  this->display->clear();
  this->display->drawXbm(0, 0, logo_width, logo_height, logo_bits);
  this->display->setFont(ArialMT_Plain_10);
  this->display->setTextAlignment(TEXT_ALIGN_LEFT);
  this->display->drawString(logo_width + 5, 5, "lora-at");
  if (status != NULL) {
    this->display->drawString(logo_width + 5, 21, "status: " + String(status));
  } else {
    this->display->drawString(logo_width + 5, 21, "status:");
  }
  this->display->display();
}

void Display::setStatus(const char *status) {
  this->status = status;
}

void Display::setEnabled(bool enabled) {
  if (!enabled && this->display != NULL) {
    this->display->resetDisplay();
  } else if (enabled) {
    this->update();
  }
  this->enabled = enabled;
}

bool Display::isEnabled() {
  return this->enabled;
}

Display::~Display() {
  if (display != NULL) {
    delete display;
  }
}