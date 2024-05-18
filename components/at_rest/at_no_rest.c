#include <at_rest.h>

struct at_rest_t {
  int dummy;
};

esp_err_t at_rest_create(sx127x_wrapper *device, at_rest **result) {
  *result = NULL;
  return ESP_OK;
}

esp_err_t at_rest_add_frame(sx127x_frame_t *frame, at_rest *handler) {
  //do nothing
  return ESP_OK;
}

void at_rest_destroy(at_rest *result) {
  //do nothing
}