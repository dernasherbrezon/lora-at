#include "at_config.h"

#include <nvs.h>
#include <nvs_flash.h>

const char *at_config_label = "lora-at";

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      return __err_rc;        \
    }                         \
  } while (0)


esp_err_t lora_at_config_create(lora_at_config_t **config) {
  lora_at_config_t *result = malloc(sizeof(lora_at_config_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  nvs_handle_t out_handle;
  ERROR_CHECK(nvs_open(at_config_label, NVS_READONLY, &out_handle));
  uint8_t display_init;
  ERROR_CHECK(nvs_get_u8(out_handle, "display_init", &display_init));
  result->init_display = display_init == 1;
  size_t float_size = sizeof(float);
  ERROR_CHECK(nvs_get_blob(out_handle, "minFrequency", &result->min_freq, &float_size));
  ERROR_CHECK(nvs_get_blob(out_handle, "maxFrequency", &result->max_freq, &float_size));
  ERROR_CHECK(nvs_get_u64(out_handle, "period", &result->bt_poll_period));
  size_t bt_address_length = sizeof(result->bt_address);
  ERROR_CHECK(nvs_get_blob(out_handle, "address", result->bt_address, &bt_address_length));
  nvs_close(out_handle);
  *config = result;
  return ESP_OK;
}

void lora_at_config_destroy(lora_at_config_t *config) {
  if (config == NULL) {
    return;
  }
  free(config);
}