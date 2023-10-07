#include <unity.h>
#include <stdlib.h>
#include "Mockat_config.h"
#include "Mockdisplay.h"
#include "Mocksx127x_util.h"
#include <at_handler.h>
#include <string.h>

at_handler_t *handler = NULL;
lora_at_config_t at_config;
char *actual_output = NULL;

void at_handler_test_callback(char *data, void *ctx) {
  actual_output = data;
}

void assert_request_response(char *request, const char *expected_response) {
  at_handler_process(request, strlen(request), at_handler_test_callback, NULL, handler);
  TEST_ASSERT_EQUAL_STRING(expected_response, actual_output);
}

void test_at_handler_AT() {
  assert_request_response("AT", "OK\r\n");
}

void test_at_handler_QUERY_AT_DISPLAY() {
  at_config.init_display = false;
  assert_request_response("AT+DISPLAY?", "0\r\nOK\r\n");
  at_config.init_display = true;
  assert_request_response("AT+DISPLAY?", "1\r\nOK\r\n");
}

void test_at_handler_SET_AT_DISPLAY() {
  lora_at_display_start_ExpectAnyArgsAndReturn(ESP_OK);
  lora_at_config_set_display_ExpectAnyArgsAndReturn(ESP_OK);
  assert_request_response("AT+DISPLAY=1", "OK\r\n");
}

void test_at_handler_QUERY_AT_MINFREQ() {
  lora_util_get_min_frequency_ExpectAndReturn(465000000);
  assert_request_response("AT+MINFREQ?", "465000000\r\nOK\r\n");
}

void test_at_handler_QUERY_AT_MAXFREQ() {
  lora_util_get_max_frequency_ExpectAndReturn(435000000);
  assert_request_response("AT+MAXFREQ?", "435000000\r\nOK\r\n");
}

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(ESP_OK, at_handler_create(&at_config, NULL, NULL, NULL, &handler));
}

void tearDown(void) {
  if (handler != NULL) {
    at_handler_destroy(handler);
    handler = NULL;
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_at_handler_AT);
  RUN_TEST(test_at_handler_QUERY_AT_DISPLAY);
  RUN_TEST(test_at_handler_SET_AT_DISPLAY);
  RUN_TEST(test_at_handler_QUERY_AT_MINFREQ);
  RUN_TEST(test_at_handler_QUERY_AT_MAXFREQ);
  exit(UNITY_END());
}