#ifndef Display_h
#define Display_h

#include <Preferences.h>
#include <SSD1306Wire.h>
#include <Wire.h>

class Display {
 public:
  ~Display();
  void init();
  void setStatus(const char *status);
  void update();
  void setEnabled(bool enabled);
  bool isEnabled();

 private:
  const char *status = NULL;
  SSD1306Wire *display = NULL;
  bool enabled;
  Preferences preferences;
};

#endif