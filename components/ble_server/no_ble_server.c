#include "ble_server.h"

esp_err_t ble_server_create(at_sensors *sensors, ble_server **server) {
  *server = NULL;
  return ESP_OK;
}

void ble_server_send_notifications(ble_server *server) {
  //do nothing
}

void ble_server_destroy(ble_server *server) {
  //do nothing
}
