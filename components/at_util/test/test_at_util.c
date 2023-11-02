#include <unity.h>
#include <stdlib.h>
#include "at_util.h"

at_util_vector_t *vector = NULL;

typedef struct {
  char data[514];
  uint64_t timestamp;
} at_util_test_t;

void assert_input(const char *input) {
  uint8_t output[514];
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, at_util_string2hex(input, output, &output_len));
  uint8_t expected[] = {0xca, 0xfe, 0x10};
  size_t expected_len = sizeof(expected) / sizeof(uint8_t);
  // there is no size_t comparison in unity.h
  TEST_ASSERT_TRUE(expected_len == output_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, output, expected_len);
}

void assert_invalid_input(const char *input) {
  uint8_t output[514];
  size_t output_len = 0;
  TEST_ASSERT_NOT_EQUAL(0, at_util_string2hex(input, output, &output_len));
}

TEST_CASE("string2hex", "[at_util]") {
  assert_input("cafe10");
  assert_input("ca fe 10");
  assert_input("   ca fe        10  ");
  assert_input("cA Fe 10");
  assert_input("cA:Fe:10");
  assert_invalid_input("caxe10");
  assert_invalid_input("caf 10");
}

TEST_CASE("hex2string", "[at_util]") {
  char str_output[8];
  uint8_t input[] = {0xca, 0xfe};
  size_t input_len = sizeof(input) / sizeof(uint8_t);
  TEST_ASSERT_EQUAL_INT(0, at_util_hex2string(input, input_len, str_output));
  TEST_ASSERT_EQUAL_STRING("CAFE", str_output);
}

TEST_CASE("vector", "[at_util]") {
  TEST_ASSERT_EQUAL(ESP_OK, at_util_vector_create(&vector));
  TEST_ASSERT_EQUAL(0, at_util_vector_size(vector));
  at_util_test_t item = {
      .data = "TEST",
      .timestamp = 1234
  };
  TEST_ASSERT_EQUAL(ESP_OK, at_util_vector_add(&item, vector));
  TEST_ASSERT_EQUAL(1, at_util_vector_size(vector));
  at_util_test_t item2 = {
      .data = "TEST2",
      .timestamp = 1235
  };
  TEST_ASSERT_EQUAL(ESP_OK, at_util_vector_add(&item2, vector));
  TEST_ASSERT_EQUAL(2, at_util_vector_size(vector));
  at_util_test_t *result = NULL;
  at_util_vector_get(0, (void *) &result, vector);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING(item.data, result->data);
  TEST_ASSERT_EQUAL(item.timestamp, result->timestamp);
  at_util_vector_get(1, (void *) &result, vector);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING(item2.data, result->data);
  TEST_ASSERT_EQUAL(item2.timestamp, result->timestamp);
  at_util_vector_clear(vector);
  TEST_ASSERT_EQUAL(0, at_util_vector_size(vector));
  at_util_vector_destroy(vector);
}
