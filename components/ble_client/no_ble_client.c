#include "ble_client.h"

esp_err_t ble_client_create(uint8_t *address, ble_client **client) {
  *client = NULL;
  //do nothing
  return ESP_OK;
}

esp_err_t ble_client_connect(uint8_t *address, ble_client *client) {
  //do nothing
  return ESP_OK;
}

esp_err_t ble_client_disconnect(ble_client *client) {
  //do nothing
  return ESP_OK;
}

esp_err_t ble_client_load_request(lora_config_t **request, ble_client *client) {
  //do nothing
  return ESP_OK;
}

esp_err_t ble_client_send_frame(lora_frame_t *frame, ble_client *client) {
  //do nothing
  return ESP_OK;
}

esp_err_t ble_client_get_rssi(ble_client *client, int8_t *rssi) {
  *rssi = -128;
  return ESP_OK;
}

esp_err_t ble_client_send_status(ble_client_status *status, ble_client *client) {
  //do nothing
  return ESP_OK;
}

void ble_client_destroy(ble_client *client) {
  //do nothing
}