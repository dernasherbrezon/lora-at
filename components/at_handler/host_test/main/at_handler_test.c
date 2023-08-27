#include <unity.h>
#include <stdlib.h>
#include <driver/Mockuart.h>
#include <Mockat_config.h>
#include <Mockdisplay.h>
#include <at_handler.h>
#include <string.h>
#include <stdarg.h>

int uart_num = 0;
at_handler_t *handler = NULL;
lora_at_config_t at_config;

void test_request_response(const char *request, int num_args, ...) {
  uart_read_bytes_ExpectAnyArgsAndReturn(strlen(request));
  uart_read_bytes_ReturnArrayThruPtr_buf((void *) request, strlen(request));
  va_list args;
  va_start (args, num_args);
  for (int i = 0; i < num_args; i++) {
    const char *expected_response = va_arg(args, const char *);
    uart_write_bytes_ExpectAndReturn(uart_num, expected_response, strlen(expected_response), 0);
  }
  va_end (args);
  uart_read_bytes_ExpectAnyArgsAndReturn(-1);
  at_handler_process(handler);
}

TEST_CASE("AT", "[at_handler]") {
  test_request_response("AT\r\n", 1, "OK\r\n");
}

TEST_CASE("AT+DISPLAY?", "[at_handler]") {
  at_config.init_display = false;
  test_request_response("AT+DISPLAY?\r\n", 2, "0\r\n", "OK\r\n");
  at_config.init_display = true;
  test_request_response("AT+DISPLAY?\r\n", 2, "1\r\n", "OK\r\n");
}

TEST_CASE("AT+DISPLAY=", "[at_handler]") {
  lora_at_display_start_ExpectAnyArgsAndReturn(ESP_OK);
  lora_at_config_set_display_ExpectAnyArgsAndReturn(ESP_OK);
  test_request_response("AT+DISPLAY=1\r\n", 1, "OK\r\n");
}

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(ESP_OK, at_handler_create(1024, uart_num, &at_config, NULL, &handler));
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