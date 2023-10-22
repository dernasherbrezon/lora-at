#include "at_timer.h"
#include <esp_attr.h>
#include <esp_log.h>
#include <inttypes.h>

#define TIMER_RESOLUTION      1000000 // 1MHz, 1 tick = 1us

static const char *TAG = "lora-at";

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      return __err_rc;        \
    }                         \
  } while (0)

#define LOG_ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
        ESP_LOGE(TAG, "unable to run %s: %s", #x, esp_err_to_name(__err_rc));     \
    }                         \
  } while (0)

bool IRAM_ATTR at_timer_interrupt_fromisr(gptimer_handle_t timer_handle, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
  at_timer_t *timer = (at_timer_t *) user_ctx;
  xTaskResumeFromISR(timer->task_handle);
  return true;
}

void at_timer_interrupt_task(void *arg) {
  at_timer_t *timer = (at_timer_t *) arg;
  while (1) {
    vTaskSuspend(NULL);
    timer->at_timer_callback(timer->callback_ctx);
  }
}

esp_err_t at_timer_create(void (*at_timer_callback)(void *arg), void *ctx, at_timer_t **timer) {
  at_timer_t *result = malloc(sizeof(at_timer_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }

  result->at_timer_callback = at_timer_callback;
  result->callback_ctx = ctx;
  result->handle = NULL;
  result->task_handle = NULL;

  *timer = result;
  return ESP_OK;
}

esp_err_t at_timer_start(uint64_t inactivity_period_micros, at_timer_t *timer) {
  at_timer_stop(timer);
  if (timer->task_handle == NULL) {
    BaseType_t task_code = xTaskCreatePinnedToCore(at_timer_interrupt_task, "handle timer", 8196, timer, 2, &timer->task_handle, xPortGetCoreID());
    if (task_code != pdPASS) {
      return ESP_ERR_INVALID_STATE;
    }
  }
  if (timer->handle == NULL) {
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_RESOLUTION,
    };
    ERROR_CHECK(gptimer_new_timer(&timer_config, &timer->handle));
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = inactivity_period_micros,
        .flags.auto_reload_on_alarm = false,
    };
    gptimer_event_callbacks_t cbs = {
        .on_alarm = at_timer_interrupt_fromisr,
    };
    ERROR_CHECK(gptimer_register_event_callbacks(timer->handle, &cbs, timer));
    ERROR_CHECK(gptimer_set_alarm_action(timer->handle, &alarm_config));
    ERROR_CHECK(gptimer_enable(timer->handle));
    ERROR_CHECK(gptimer_start(timer->handle));
    ESP_LOGI(TAG, "inactivity timer started: %.2fs", inactivity_period_micros / 1000000.0F);
  }
  return ESP_OK;
}

esp_err_t at_timer_set_counter(uint64_t current_time, at_timer_t *timer) {
  ERROR_CHECK(gptimer_set_raw_count(timer->handle, current_time));
  return gptimer_start(timer->handle);
}

esp_err_t at_timer_get_counter(uint64_t *output, at_timer_t *timer) {
  //handle gracefully
  //this function can be called from uart_at even if timer is not actually initialized
  if (timer->handle == NULL) {
    *output = 0;
    return ESP_OK;
  }
  esp_err_t  result = gptimer_get_raw_count(timer->handle, output);
  return result;
}

esp_err_t at_timer_stop(at_timer_t *timer) {
  bool actually_stopped = false;
  if (timer->handle != NULL) {
    LOG_ERROR_CHECK(gptimer_disable(timer->handle));
    LOG_ERROR_CHECK(gptimer_del_timer(timer->handle));
    timer->handle = NULL;
    actually_stopped = true;
  }
  if (timer->task_handle != NULL) {
    vTaskDelete(timer->task_handle);
    timer->task_handle = NULL;
    actually_stopped = true;
  }
  if (actually_stopped) {
    ESP_LOGI(TAG, "inactivity timer stopped");
  }
  return ESP_OK;
}

void at_timer_destroy(at_timer_t *timer) {
  if (timer == NULL) {
    return;
  }
  at_timer_stop(timer);
  free(timer);
}