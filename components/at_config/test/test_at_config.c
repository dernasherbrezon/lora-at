#include <unity.h>
#include <stdlib.h>
#include <nvs_flash.h>
#include "at_config.h"

lora_at_config_t *at_config = NULL;

uint8_t bt_address[] = { 0x30, 0x83, 0x98, 0xdb, 0x6c, 0xfe };

TEST_CASE("init empty", "[at_config]") {
  ESP_ERROR_CHECK(nvs_flash_erase());
  ESP_ERROR_CHECK(lora_at_config_create(&at_config));
  TEST_ASSERT_FALSE(at_config->init_display);
  TEST_ASSERT_EQUAL(0, at_config->inactivity_period_micros);
  TEST_ASSERT_EQUAL(0, at_config->deep_sleep_period_micros);
  TEST_ASSERT_NULL(at_config->bt_address);
  lora_at_config_destroy(at_config);
}

void test_at_config_assert_config() {
  TEST_ASSERT_TRUE(at_config->init_display);
  TEST_ASSERT_EQUAL(30000, at_config->inactivity_period_micros);
  TEST_ASSERT_EQUAL(60000, at_config->deep_sleep_period_micros);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(bt_address, at_config->bt_address, sizeof(bt_address) / sizeof(uint8_t));
}

TEST_CASE("set config", "[at_config]") {
  ESP_ERROR_CHECK(lora_at_config_create(&at_config));
  ESP_ERROR_CHECK(lora_at_config_set_display(true, at_config));
  ESP_ERROR_CHECK(lora_at_config_set_bt_address(bt_address, sizeof(bt_address), at_config));
  ESP_ERROR_CHECK(lora_at_config_set_dsconfig(30000, 60000, at_config));
  test_at_config_assert_config();
  lora_at_config_destroy(at_config);
  ESP_ERROR_CHECK(lora_at_config_create(&at_config));
  test_at_config_assert_config();
  ESP_ERROR_CHECK(lora_at_config_set_bt_address(NULL, 0, at_config));
  lora_at_config_destroy(at_config);
}


