#include <unity.h>
#include "display.h"

lora_at_display *display = NULL;

TEST_CASE("success", "[display]") {
  ESP_ERROR_CHECK(lora_at_display_create(&display));
  ESP_ERROR_CHECK(lora_at_display_start(display));
  // check double start
  ESP_ERROR_CHECK(lora_at_display_start(display));
  ESP_ERROR_CHECK(lora_at_display_set_status("IDLE", display));
  ESP_ERROR_CHECK(lora_at_display_stop(display));
  // check double stop
  ESP_ERROR_CHECK(lora_at_display_stop(display));
  lora_at_display_destroy(display);
}