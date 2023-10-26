#include <unity.h>
#include <stdlib.h>
#include <nvs_flash.h>
#include "at_config.h"

lora_at_config_t *at_config = NULL;

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
  TEST_ASSERT_EQUAL_STRING("30:83:98:db:6c:fe", at_config->bt_address);
}

TEST_CASE("set config", "[at_config]") {
  ESP_ERROR_CHECK(lora_at_config_create(&at_config));
  ESP_ERROR_CHECK(lora_at_config_set_display(true, at_config));
  ESP_ERROR_CHECK(lora_at_config_set_bt_address("30:83:98:db:6c:fe", at_config));
  ESP_ERROR_CHECK(lora_at_config_set_dsconfig(30000, 60000, at_config));
  test_at_config_assert_config();
  lora_at_config_destroy(at_config);
  ESP_ERROR_CHECK(lora_at_config_create(&at_config));
  test_at_config_assert_config();
  ESP_ERROR_CHECK(lora_at_config_set_bt_address(NULL, at_config));
  lora_at_config_destroy(at_config);
}


