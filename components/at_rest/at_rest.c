#include "at_rest.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>

#define TEMP_BUFFER_LENGTH 1024
#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) { \
      at_rest_destroy(result);                        \
      return __err_rc;        \
    }                         \
  } while (0)


static const char *TAG = "at_rest";

struct at_rest_t {
  sx127x *device;
  httpd_handle_t server;
  char temp_buffer[TEMP_BUFFER_LENGTH];
};

esp_err_t at_rest_respond(const char *status, const char *status_message, httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "status", status);
  if (status_message != NULL) {
    cJSON_AddStringToObject(root, "failureMessage", status_message);
  }
  const char *response = cJSON_Print(root);
  esp_err_t result = httpd_resp_sendstr(req, response);
  free((void *) response);
  cJSON_Delete(root);
  return result;
}

esp_err_t at_rest_read_body(httpd_req_t *req) {
  size_t total_len = req->content_len;
  if (total_len == 0) {
    return at_rest_respond("FAILURE", "request is empty", req);
  }
  if (total_len >= TEMP_BUFFER_LENGTH) {
    return at_rest_respond("FAILURE", "content is too long", req);
  }
  size_t cur_len = 0;
  int received = 0;
  at_rest *rest = (at_rest *) req->user_ctx;
  while (cur_len < total_len) {
    received = httpd_req_recv(req, rest->temp_buffer + cur_len, total_len);
    if (received <= 0) {
      return at_rest_respond("FAILURE", "unable to read body", req);
    }
    cur_len += received;
  }
  rest->temp_buffer[total_len] = '\0';
  return ESP_OK;
}

static esp_err_t at_rest_lora_rx_start(httpd_req_t *req) {
  esp_err_t code = at_rest_read_body(req);
  if (code != ESP_OK) {
    return code;
  }
  at_rest *rest = (at_rest *) req->user_ctx;
  cJSON *root = cJSON_Parse(rest->temp_buffer);
  if (root == NULL) {
    return at_rest_respond("FAILURE", "unable to parse request", req);
  }
  lora_config_t lora_req;
  lora_req.freq = (uint64_t) cJSON_GetObjectItem(root, "freq")->valuedouble;
  //FIXME convert more
  cJSON_Delete(root);
  code = sx127x_util_lora_rx(SX127x_MODE_RX_CONT, &lora_req, rest->device);
  if (code != ESP_OK) {
    return at_rest_respond("FAILURE", "unable to rx", req);
  }
  return at_rest_respond("SUCCESS", NULL, req);
}

esp_err_t at_rest_create(sx127x *device, at_rest **rest) {
  struct at_rest_t *result = malloc(sizeof(struct at_rest_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->device = device;
  result->server = NULL;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGI(TAG, "Starting HTTP Server");
  ERROR_CHECK(httpd_start(&result->server, &config));

  httpd_uri_t lora_rx_start_uri = {
      .uri = "/lora/rx/start",
      .method = HTTP_POST,
      .handler = at_rest_lora_rx_start,
      .user_ctx = result
  };
  ERROR_CHECK(httpd_register_uri_handler(result->server, &lora_rx_start_uri));

  *rest = result;
  return ESP_OK;
}

void at_rest_destroy(at_rest *result) {
  if (result == NULL) {
    return;
  }
  if (result->server != NULL) {
    httpd_stop(result->server);
  }
  free(result);
}
