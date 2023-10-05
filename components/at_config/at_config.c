#include "at_config.h"
#include <nvs_flash.h>
#include <string.h>

const char *at_config_label = "lora-at";

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      return __err_rc;        \
    }                         \
  } while (0)

#define ERROR_CHECK_IGNORE_NOT_FOUND(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK && __err_rc != ESP_ERR_NVS_NOT_FOUND) {      \
      return __err_rc;        \
    }                         \
  } while (0)


esp_err_t lora_at_config_create(lora_at_config_t **config) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ERROR_CHECK(nvs_flash_erase());
    ERROR_CHECK(nvs_flash_init());
  }
  lora_at_config_t *result = malloc(sizeof(lora_at_config_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  nvs_handle_t out_handle;
  err = nvs_open(at_config_label, NVS_READONLY, &out_handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    *result = (lora_at_config_t) {0};
    *config = result;
    return ESP_OK;
  } else if (err != ESP_OK) {
    return err;
  }
  uint8_t display_init;
  ERROR_CHECK_IGNORE_NOT_FOUND(nvs_get_u8(out_handle, "display_init", &display_init));
  result->init_display = display_init == 1;
  size_t uint64_t_size = sizeof(uint64_t);
  ERROR_CHECK_IGNORE_NOT_FOUND(nvs_get_u64(out_handle, "period", &result->bt_poll_period));
  size_t bt_address_length = sizeof(result->bt_address);
  ERROR_CHECK_IGNORE_NOT_FOUND(nvs_get_blob(out_handle, "address", result->bt_address, &bt_address_length));
  nvs_close(out_handle);
  *config = result;
  return ESP_OK;
}

esp_err_t lora_at_config_set_display(bool init_display, lora_at_config_t *config) {
  nvs_handle_t out_handle;
  ERROR_CHECK(nvs_open(at_config_label, NVS_READWRITE, &out_handle));
  ERROR_CHECK(nvs_set_u8(out_handle, "display_init", init_display));
  nvs_close(out_handle);
  config->init_display = init_display;
  return ESP_OK;
}

esp_err_t lora_at_config_set_bt_address(char *bt_address, lora_at_config_t *config) {
  nvs_handle_t out_handle;
  ERROR_CHECK(nvs_open(at_config_label, NVS_READWRITE, &out_handle));
  ERROR_CHECK(nvs_set_blob(out_handle, "address", &bt_address, strlen(bt_address)));
  nvs_close(out_handle);
  memcpy(config->bt_address, bt_address, sizeof(config->bt_address));
  return ESP_OK;
}

void lora_at_config_destroy(lora_at_config_t *config) {
  if (config == NULL) {
    return;
  }
  free(config);
  nvs_flash_deinit();
}