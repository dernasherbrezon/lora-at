#include <unity.h>
#include <stdlib.h>
#include <Mockat_config.h>
#include <Mockdisplay.h>
#include <at_handler.h>
#include <string.h>

at_handler_t *handler = NULL;
lora_at_config_t at_config;

void test_request_response(char *request, const char *expected_response) {
  char *output = NULL;
  size_t output_length = 0;
  at_handler_process(request, strlen(request), &output, &output_length, handler);
  TEST_ASSERT_EQUAL_STRING(expected_response, output);
}

TEST_CASE("AT", "[at_handler]") {
  test_request_response("AT", "OK\r\n");
}

TEST_CASE("AT+DISPLAY?", "[at_handler]") {
  at_config.init_display = false;
  test_request_response("AT+DISPLAY?", "0\r\nOK\r\n");
  at_config.init_display = true;
  test_request_response("AT+DISPLAY?", "1\r\nOK\r\n");
}

TEST_CASE("AT+DISPLAY=", "[at_handler]") {
  lora_at_display_start_ExpectAnyArgsAndReturn(ESP_OK);
  lora_at_config_set_display_ExpectAnyArgsAndReturn(ESP_OK);
  test_request_response("AT+DISPLAY=1", "OK\r\n");
}

TEST_CASE("AT+MINFREQ?", "[at_handler]") {
  at_config.min_freq = 465000000;
  test_request_response("AT+MINFREQ?", "465000000\r\nOK\r\n");
}

TEST_CASE("AT+MINFREQ=", "[at_handler]") {
  lora_at_config_set_min_freq_ExpectAndReturn(433000000, &at_config, ESP_OK);
  test_request_response("AT+MINFREQ=433000000", "OK\r\n");
}

TEST_CASE("AT+MAXFREQ?", "[at_handler]") {
  at_config.max_freq = 435000000;
  test_request_response("AT+MAXFREQ?", "435000000\r\nOK\r\n");
}

TEST_CASE("AT+MAXFREQ=", "[at_handler]") {
  lora_at_config_set_max_freq_ExpectAndReturn(433000000, &at_config, ESP_OK);
  test_request_response("AT+MAXFREQ=433000000", "OK\r\n");
}

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(ESP_OK, at_handler_create(&at_config, NULL, &handler));
}

void tearDown(void) {
  if (handler != NULL) {
    at_handler_destroy(handler);
    handler = NULL;
  }
}

void app_main(void) {
  UNITY_BEGIN();
  unity_run_all_tests();
  exit(UNITY_END());
}