#include <unity.h>
#include <stdlib.h>
#include <driver/Mockuart.h>
#include <at_handler.h>
#include <string.h>

int uart_num = 0;
const char *at_request = "AT\r\n";
const char *at_success_response = "OK\r\n";
at_handler_t *handler = NULL;

TEST_CASE("basic test", "[at_handler]") {
uart_read_bytes_ExpectAnyArgsAndReturn(strlen(at_request));
uart_read_bytes_ReturnArrayThruPtr_buf((void *) at_request, strlen(at_request));
uart_write_bytes_ExpectAndReturn(uart_num, at_success_response, strlen(at_success_response), 0);
uart_read_bytes_ExpectAnyArgsAndReturn(-1);
at_handler_process(handler);
}

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(ESP_OK, at_handler_create(1024, uart_num, &handler));
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