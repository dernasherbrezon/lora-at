#include <unity.h>
#include <at_timer.h>
#include <freertos/semphr.h>

at_timer_t *timer = NULL;
SemaphoreHandle_t test_at_timer;
const char *ctx = "test";
void *actual_ctx = NULL;
uint64_t inactivity_timeout = 30000;

void at_timer_callback(void *ctx) {
  actual_ctx = ctx;
  xSemaphoreGive(test_at_timer);
}

TEST_CASE("success", "[at_timer]") {
  test_at_timer = xSemaphoreCreateBinary();
  ESP_ERROR_CHECK(at_timer_create(at_timer_callback, (void *) ctx, &timer));
  ESP_ERROR_CHECK(at_timer_start(inactivity_timeout, timer));
  xSemaphoreTake(test_at_timer, inactivity_timeout);
  TEST_ASSERT_EQUAL_STRING(ctx, actual_ctx);

  uint64_t output = 0;
  ESP_ERROR_CHECK(at_timer_get_counter(&output, timer));
  TEST_ASSERT_MESSAGE(output >= inactivity_timeout, "wait too small");
  // just make sure set works
  ESP_ERROR_CHECK(at_timer_set_counter(123, timer));
  ESP_ERROR_CHECK(at_timer_stop(timer));
  vSemaphoreDelete(test_at_timer);
  at_timer_destroy(timer);
}