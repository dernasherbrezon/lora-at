#include <unity.h>
#include <esp_heap_trace.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <driver/i2c.h>
#include <driver/gptimer.h>

#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM

void setUp() {
  ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
}

void tearDown() {
  ESP_ERROR_CHECK(heap_trace_stop());
  heap_trace_dump();
  TEST_ASSERT_MESSAGE(heap_trace_get_count() == 0, "memory leak");
}

void app_main(void) {
  // some workaround to solve false-positive memory leaks
  // some esp-idf structures initialized but never freed
  // this is normally not a problem because they are normally singletons and allocate several bytes
  ESP_LOGI("unit", "start unit test. tracing memory: %.2f", NUM_RECORDS * 1.0F);
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0));
  ESP_ERROR_CHECK(i2c_driver_delete(I2C_NUM_1));
  ESP_ERROR_CHECK(nvs_flash_erase());
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,
  };
  gptimer_handle_t handle;
  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &handle));
  ESP_ERROR_CHECK(gptimer_del_timer(handle));

  // actual tests
  ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
  unity_run_menu();
}