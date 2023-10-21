#ifndef at_timer_h
#define at_timer_h

#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gptimer.h>

typedef struct {
  void *callback_ctx;
  void (*at_timer_callback)(void *arg);
  gptimer_handle_t handle;
  TaskHandle_t task_handle;
} at_timer_t;

esp_err_t at_timer_create(void (*at_timer_callback)(void *arg), void *ctx, at_timer_t **timer);

esp_err_t at_timer_start(uint64_t inactivity_period_micros, at_timer_t *timer);

esp_err_t at_timer_set_counter(uint64_t current_time, at_timer_t *timer);

esp_err_t at_timer_get_counter(uint64_t *output, at_timer_t *timer);

esp_err_t at_timer_stop(at_timer_t *timer);

void at_timer_destroy(at_timer_t *timer);

#endif