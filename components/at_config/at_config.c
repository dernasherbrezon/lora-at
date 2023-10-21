#include "at_config.h"
#include <nvs_flash.h>
#include <string.h>

#define BT_ADDRESS_LENGTH 18

const char *at_config_label = "lora-at";

#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      nvs_close(out_handle);                         \
      return __err_rc;        \
    }                         \
  } while (0)

#define ERROR_CHECK_RETURN(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      return __err_rc;        \
    }                         \
  } while (0)


#define ERROR_CHECK_IGNORE_NOT_FOUND(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK && __err_rc != ESP_ERR_NVS_NOT_FOUND) { \
      nvs_close(out_handle);                                         \
      lora_at_config_destroy(result);                                         \
      return __err_rc;        \
    }                         \
  } while (0)


esp_err_t lora_at_config_read_bt_address(nvs_handle_t out_handle, lora_at_config_t *config) {
  char bt_address[BT_ADDRESS_LENGTH];
  size_t bt_address_length = sizeof(bt_address);
  memset(bt_address, 0, bt_address_length);
  esp_err_t err = nvs_get_blob(out_handle, "address", bt_address, &bt_address_length);
  if (err == ESP_OK) {
    config->bt_address = malloc(BT_ADDRESS_LENGTH);
    if (config->bt_address == NULL) {
      return ESP_ERR_NO_MEM;
    }
    memcpy(config->bt_address, bt_address, BT_ADDRESS_LENGTH);
  } else if (err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }
  return ESP_OK;
}

esp_err_t lora_at_config_create(lora_at_config_t **config) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ERROR_CHECK_RETURN(nvs_flash_erase());
    ERROR_CHECK_RETURN(nvs_flash_init());
  }
  lora_at_config_t *result = malloc(sizeof(lora_at_config_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  *result = (lora_at_config_t) {0};
  nvs_handle_t out_handle;
  err = nvs_open(at_config_label, NVS_READONLY, &out_handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    *config = result;
    return ESP_OK;
  } else if (err != ESP_OK) {
    lora_at_config_destroy(result);
    return err;
  }
  uint8_t display_init;
  ERROR_CHECK_IGNORE_NOT_FOUND(nvs_get_u8(out_handle, "display_init", &display_init));
  result->init_display = (display_init == 1);
  ERROR_CHECK_IGNORE_NOT_FOUND(nvs_get_u64(out_handle, "period", &result->deep_sleep_period_micros));
  ERROR_CHECK_IGNORE_NOT_FOUND(nvs_get_u64(out_handle, "inactivity", &result->inactivity_period_micros));
  ERROR_CHECK_IGNORE_NOT_FOUND(lora_at_config_read_bt_address(out_handle, result));
  nvs_close(out_handle);
  *config = result;
  return ESP_OK;
}

esp_err_t lora_at_config_set_display(bool init_display, lora_at_config_t *config) {
  nvs_handle_t out_handle;
  ERROR_CHECK(nvs_open(at_config_label, NVS_READWRITE, &out_handle));
  ERROR_CHECK(nvs_set_u8(out_handle, "display_init", init_display));
  ERROR_CHECK(nvs_commit(out_handle));
  nvs_close(out_handle);
  config->init_display = init_display;
  return ESP_OK;
}

esp_err_t lora_at_config_set_dsconfig(uint64_t inactivity_period_micros, uint64_t deep_sleep_period_micros, lora_at_config_t *config) {
  nvs_handle_t out_handle;
  ERROR_CHECK(nvs_open(at_config_label, NVS_READWRITE, &out_handle));
  ERROR_CHECK(nvs_set_u64(out_handle, "inactivity", inactivity_period_micros));
  ERROR_CHECK(nvs_set_u64(out_handle, "period", deep_sleep_period_micros));
  ERROR_CHECK(nvs_commit(out_handle));
  nvs_close(out_handle);
  config->inactivity_period_micros = inactivity_period_micros;
  config->deep_sleep_period_micros = deep_sleep_period_micros;
  return ESP_OK;
}

esp_err_t lora_at_config_set_bt_address(char *bt_address, lora_at_config_t *config) {
  if (bt_address != NULL) {
    nvs_handle_t out_handle;
    ERROR_CHECK(nvs_open(at_config_label, NVS_READWRITE, &out_handle));
    ERROR_CHECK(nvs_set_blob(out_handle, "address", bt_address, BT_ADDRESS_LENGTH));
    ERROR_CHECK(nvs_commit(out_handle));
    nvs_close(out_handle);
    if (config->bt_address == NULL) {
      config->bt_address = malloc(BT_ADDRESS_LENGTH);
      if (config->bt_address == NULL) {
        return ESP_ERR_NO_MEM;
      }
      memset(config->bt_address, 0, BT_ADDRESS_LENGTH);
    }
    memcpy(config->bt_address, bt_address, BT_ADDRESS_LENGTH);
  } else {
    if (config->bt_address != NULL) {
      nvs_handle_t out_handle;
      ERROR_CHECK(nvs_open(at_config_label, NVS_READWRITE, &out_handle));
      ERROR_CHECK(nvs_erase_key(out_handle, "address"));
      ERROR_CHECK(nvs_commit(out_handle));
      nvs_close(out_handle);
      free(config->bt_address);
      config->bt_address = NULL;
    }
  }
  return ESP_OK;
}

void lora_at_config_destroy(lora_at_config_t *config) {
  if (config == NULL) {
    return;
  }
  if (config->bt_address != NULL) {
    free(config->bt_address);
  }
  free(config);
  nvs_flash_deinit();
}