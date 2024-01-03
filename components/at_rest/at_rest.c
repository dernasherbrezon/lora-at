#include "at_rest.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <at_util.h>
#include <esp_tls_crypto.h>
#include "sdkconfig.h"

#ifndef CONFIG_AT_API_USERNAME
#define CONFIG_AT_API_USERNAME "r2lora"
#endif

#ifndef CONFIG_AT_API_PASSWORD
#define CONFIG_AT_API_PASSWORD ""
#endif

#define TEMP_BUFFER_LENGTH 1024
#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) { \
      at_rest_destroy(result);                        \
      return __err_rc;        \
    }                         \
  } while (0)

#define ERROR_CHECK_RETURN(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) { \
      return __err_rc;        \
    }                         \
  } while (0)


static const char *TAG = "at_rest";

struct at_rest_t {
  sx127x *device;
  httpd_handle_t server;
  at_util_vector_t *frames;
  sx127x_modulation_t active_mode;
  char *digest;
  char temp_buffer[TEMP_BUFFER_LENGTH];
};

esp_err_t at_rest_respond_auth_failure(httpd_req_t *req) {
  ERROR_CHECK_RETURN(httpd_resp_set_status(req, "401 UNAUTHORIZED"));
  ERROR_CHECK_RETURN(httpd_resp_set_type(req, "application/json"));
  ERROR_CHECK_RETURN(httpd_resp_set_hdr(req, "Connection", "keep-alive"));
  ERROR_CHECK_RETURN(httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"lora-at\""));
  ERROR_CHECK_RETURN(httpd_resp_send(req, NULL, 0));
  return ESP_OK;
}

esp_err_t at_rest_authenticate(httpd_req_t *req) {
  size_t buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
  if (buf_len <= 1) {
    return at_rest_respond_auth_failure(req);
  }
  at_rest *rest = (at_rest *) req->user_ctx;
  ERROR_CHECK_RETURN(httpd_req_get_hdr_value_str(req, "Authorization", rest->temp_buffer, buf_len));
  if (strncmp(rest->digest, rest->temp_buffer, buf_len)) {
    ESP_LOGI(TAG, "authentication failed");
    return ESP_ERR_INVALID_ARG;
  }
  return ESP_OK;
}

esp_err_t at_rest_respond(const char *status, const char *status_message, httpd_req_t *req) {
  esp_err_t code = httpd_resp_set_type(req, "application/json");
  if (code != ESP_OK) {
    return code;
  }
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "status", status);
  if (status_message != NULL) {
    cJSON_AddStringToObject(root, "failureMessage", status_message);
  }
  const char *response = cJSON_Print(root);
  code = httpd_resp_sendstr(req, response);
  free((void *) response);
  cJSON_Delete(root);
  return code;
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

static esp_err_t at_rest_status(httpd_req_t *req) {
  ERROR_CHECK_RETURN(at_rest_authenticate(req));
  return at_rest_respond("SUCCESS", NULL, req);
}

static esp_err_t at_rest_rx_pull(httpd_req_t *req) {
  ERROR_CHECK_RETURN(at_rest_authenticate(req));
  esp_err_t code = httpd_resp_set_type(req, "application/json");
  if (code != ESP_OK) {
    return code;
  }
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "status", "SUCCESS");
  at_rest *rest = (at_rest *) req->user_ctx;
  cJSON *frames = cJSON_AddArrayToObject(root, "frames");
  for (size_t i = 0; i < at_util_vector_size(rest->frames); i++) {
    lora_frame_t *cur_frame = NULL;
    at_util_vector_get(i, (void *) &cur_frame, rest->frames);
    code = at_util_hex2string(cur_frame->data, cur_frame->data_length, rest->temp_buffer);
    if (code != ESP_OK) {
      ESP_LOGE(TAG, "unable to serialize string");
      sx127x_util_frame_destroy(cur_frame);
      continue;
    }
    cJSON *cur_item = cJSON_CreateObject();
    cJSON_AddStringToObject(cur_item, "data", rest->temp_buffer);
    cJSON_AddNumberToObject(cur_item, "rssi", cur_frame->rssi);
    cJSON_AddNumberToObject(cur_item, "snr", cur_frame->snr);
    cJSON_AddNumberToObject(cur_item, "frequencyError", cur_frame->frequency_error);
    cJSON_AddNumberToObject(cur_item, "timestamp", cur_frame->timestamp);
    cJSON_AddItemToArray(frames, cur_item);
    sx127x_util_frame_destroy(cur_frame);
  }
  at_util_vector_clear(rest->frames);
  const char *response = cJSON_Print(root);
  code = httpd_resp_sendstr(req, response);
  free((void *) response);
  cJSON_Delete(root);
  return code;
}

static esp_err_t at_rest_rx_stop(httpd_req_t *req) {
  ERROR_CHECK_RETURN(at_rest_authenticate(req));
  esp_err_t code = at_rest_rx_pull(req);
  if (code != ESP_OK) {
    return at_rest_respond("FAILURE", "Unable to handle", req);
  }
  at_rest *rest = (at_rest *) req->user_ctx;
  code = sx127x_set_opmod(SX127x_MODE_SLEEP, rest->active_mode, rest->device);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to stop rx: %d", code);
  }
  return ESP_OK;
}

static void at_rest_read_request(lora_config_t *lora_req, cJSON *root) {
  lora_req->freq = (uint64_t) cJSON_GetObjectItem(root, "freq")->valuedouble;
  lora_req->bw = (uint32_t) cJSON_GetObjectItem(root, "bw")->valuedouble;
  lora_req->sf = (uint8_t) cJSON_GetObjectItem(root, "sf")->valueint;
  lora_req->cr = (uint8_t) cJSON_GetObjectItem(root, "cr")->valueint;
  lora_req->syncWord = (uint8_t) cJSON_GetObjectItem(root, "syncWord")->valueint;
  lora_req->preambleLength = (uint16_t) cJSON_GetObjectItem(root, "preambleLength")->valueint;
  lora_req->ldo = (uint8_t) cJSON_GetObjectItem(root, "ldo")->valueint;
  lora_req->useCrc = (uint8_t) cJSON_GetObjectItem(root, "useCrc")->valueint;
  lora_req->useExplicitHeader = (uint8_t) cJSON_GetObjectItem(root, "useExplicitHeader")->valueint;
  lora_req->length = (uint8_t) cJSON_GetObjectItem(root, "length")->valueint;
  // rx params
  if (cJSON_HasObjectItem(root, "gain")) {
    lora_req->gain = (uint8_t) cJSON_GetObjectItem(root, "gain")->valueint;
  }
  // tx params
  if (cJSON_HasObjectItem(root, "power")) {
    lora_req->power = (int8_t) cJSON_GetObjectItem(root, "power")->valueint;
  }
  if (cJSON_HasObjectItem(root, "ocp")) {
    lora_req->ocp = (int16_t) cJSON_GetObjectItem(root, "ocp")->valueint;
  }
  if (cJSON_HasObjectItem(root, "pin")) {
    lora_req->pin = (uint8_t) cJSON_GetObjectItem(root, "pin")->valueint;
  }
}

static void at_rest_read_fsk_request(fsk_config_t *fsk_req, cJSON *root, uint8_t *syncword) {
  fsk_req->freq = (uint64_t) cJSON_GetObjectItem(root, "freq")->valuedouble;
  fsk_req->bitrate = (uint32_t) cJSON_GetObjectItem(root, "bitrate")->valuedouble;
  fsk_req->freq_deviation = (uint32_t) cJSON_GetObjectItem(root, "freqDeviation")->valuedouble;
  fsk_req->preamble = (uint16_t) cJSON_GetObjectItem(root, "preamble")->valuedouble;
  size_t syncword_length = 0;
  at_util_string2hex(cJSON_GetObjectItem(root, "syncword")->valuestring, syncword, &syncword_length);
  fsk_req->syncword = syncword;
  fsk_req->syncword_length = (uint8_t) syncword_length;
  fsk_req->encoding = (uint8_t) cJSON_GetObjectItem(root, "encoding")->valueint;
  fsk_req->data_shaping = (uint8_t) cJSON_GetObjectItem(root, "dataShaping")->valueint;
  fsk_req->crc = (uint8_t) cJSON_GetObjectItem(root, "crc")->valueint;
  // rx
  if (cJSON_HasObjectItem(root, "rxBandwidth")) {
    fsk_req->rx_bandwidth = (uint32_t) cJSON_GetObjectItem(root, "rxBandwidth")->valueint;
  }
  if (cJSON_HasObjectItem(root, "rxAfcBandwidth")) {
    fsk_req->rx_afc_bandwidth = (uint32_t) cJSON_GetObjectItem(root, "rxAfcBandwidth")->valueint;
  }
  // tx
  if (cJSON_HasObjectItem(root, "power")) {
    fsk_req->power = (int8_t) cJSON_GetObjectItem(root, "power")->valueint;
  }
  if (cJSON_HasObjectItem(root, "ocp")) {
    fsk_req->ocp = (int16_t) cJSON_GetObjectItem(root, "ocp")->valueint;
  }
  if (cJSON_HasObjectItem(root, "pin")) {
    fsk_req->pin = (uint8_t) cJSON_GetObjectItem(root, "pin")->valueint;
  }
}

static esp_err_t at_rest_fsk_tx(httpd_req_t *req) {
  ERROR_CHECK_RETURN(at_rest_authenticate(req));
  esp_err_t code = at_rest_read_body(req);
  if (code != ESP_OK) {
    return code;
  }
  at_rest *rest = (at_rest *) req->user_ctx;
  cJSON *root = cJSON_Parse(rest->temp_buffer);
  if (root == NULL) {
    return at_rest_respond("FAILURE", "unable to parse request", req);
  }
  fsk_config_t fsk_req;
  uint8_t syncword_hex[16];
  at_rest_read_fsk_request(&fsk_req, root, syncword_hex);
  size_t message_hex_length = 0;
  uint8_t message_hex[255];
  code = at_util_string2hex(cJSON_GetObjectItem(root, "data")->valuestring, message_hex, &message_hex_length);
  cJSON_Delete(root);
  if (code != ESP_OK) {
    return at_rest_respond("FAILURE", "unable to convert data to hex", req);
  }
  if (rest->active_mode != SX127x_MODULATION_FSK) {
    sx127x_set_opmod(SX127x_MODE_SLEEP, rest->active_mode, rest->device);
  }
  sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_FSK, rest->device);
  rest->active_mode = SX127x_MODULATION_FSK;
  code = sx127x_util_fsk_rx(&fsk_req, rest->device);
  if (code != ESP_OK) {
    return at_rest_respond("FAILURE", "unable to start tx", req);
  }
  return at_rest_respond("SUCCESS", NULL, req);
}

static esp_err_t at_rest_fsk_rx_start(httpd_req_t *req) {
  ERROR_CHECK_RETURN(at_rest_authenticate(req));
  esp_err_t code = at_rest_read_body(req);
  if (code != ESP_OK) {
    return code;
  }
  at_rest *rest = (at_rest *) req->user_ctx;
  cJSON *root = cJSON_Parse(rest->temp_buffer);
  if (root == NULL) {
    return at_rest_respond("FAILURE", "unable to parse request", req);
  }
  fsk_config_t fsk_req;
  uint8_t syncword_hex[16];
  at_rest_read_fsk_request(&fsk_req, root, syncword_hex);
  cJSON_Delete(root);
  if (rest->active_mode != SX127x_MODULATION_FSK) {
    sx127x_set_opmod(SX127x_MODE_SLEEP, rest->active_mode, rest->device);
  }
  sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_FSK, rest->device);
  rest->active_mode = SX127x_MODULATION_FSK;
  code = sx127x_util_fsk_rx(&fsk_req, rest->device);
  if (code != ESP_OK) {
    return at_rest_respond("FAILURE", "unable to rx", req);
  }
  return at_rest_respond("SUCCESS", NULL, req);
}

static esp_err_t at_rest_lora_tx(httpd_req_t *req) {
  ERROR_CHECK_RETURN(at_rest_authenticate(req));
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
  at_rest_read_request(&lora_req, root);
  size_t message_hex_length = 0;
  uint8_t message_hex[255];
  code = at_util_string2hex(cJSON_GetObjectItem(root, "data")->valuestring, message_hex, &message_hex_length);
  cJSON_Delete(root);
  if (code != ESP_OK) {
    return at_rest_respond("FAILURE", "unable to convert data to hex", req);
  }
  if (rest->active_mode != SX127x_MODULATION_LORA) {
    sx127x_set_opmod(SX127x_MODE_SLEEP, rest->active_mode, rest->device);
  }
  sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, rest->device);
  rest->active_mode = SX127x_MODULATION_LORA;
  code = sx127x_util_lora_tx(message_hex, message_hex_length, &lora_req, rest->device);
  if (code != ESP_OK) {
    return at_rest_respond("FAILURE", "unable to start tx", req);
  }
  return at_rest_respond("SUCCESS", NULL, req);
}

static esp_err_t at_rest_lora_rx_start(httpd_req_t *req) {
  ERROR_CHECK_RETURN(at_rest_authenticate(req));
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
  at_rest_read_request(&lora_req, root);
  cJSON_Delete(root);
  if (rest->active_mode != SX127x_MODULATION_LORA) {
    sx127x_set_opmod(SX127x_MODE_SLEEP, rest->active_mode, rest->device);
  }
  sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, rest->device);
  rest->active_mode = SX127x_MODULATION_LORA;
  code = sx127x_util_lora_rx(SX127x_MODE_RX_CONT, &lora_req, rest->device);
  if (code != ESP_OK) {
    return at_rest_respond("FAILURE", "unable to rx", req);
  }
  return at_rest_respond("SUCCESS", NULL, req);
}

static esp_err_t at_rest_digest(const char *username, const char *password, char **result) {
  char *user_info = NULL;
  int rc = asprintf(&user_info, "%s:%s", username, password);
  if (rc < 0) {
    return ESP_ERR_NO_MEM;
  }
  if (!user_info) {
    return ESP_ERR_NO_MEM;
  }
  size_t n = 0;
  esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *) user_info, strlen(user_info));

  /* 6: The length of the "Basic " string
   * n: Number of bytes for a base64 encode format
   * 1: Number of bytes for a reserved which be used to fill zero
  */
  char *digest = calloc(1, 6 + n + 1);
  if (digest) {
    strcpy(digest, "Basic ");
    size_t out;
    esp_crypto_base64_encode((unsigned char *) digest + 6, n, &out, (const unsigned char *) user_info, strlen(user_info));
  }
  free(user_info);
  *result = digest;
  return ESP_OK;
}

esp_err_t at_rest_create(sx127x *device, at_rest **rest) {
  struct at_rest_t *result = malloc(sizeof(struct at_rest_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->device = device;
  result->server = NULL;
  result->active_mode = SX127x_MODULATION_FSK;
  result->digest = NULL;

  ERROR_CHECK(at_rest_digest(CONFIG_AT_API_USERNAME, CONFIG_AT_API_PASSWORD, &result->digest));

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGI(TAG, "Starting HTTP Server");
  ERROR_CHECK(httpd_start(&result->server, &config));

  httpd_uri_t lora_rx_start_uri = {
      .uri = "/api/v2/lora/rx/start",
      .method = HTTP_POST,
      .handler = at_rest_lora_rx_start,
      .user_ctx = result
  };
  ERROR_CHECK(httpd_register_uri_handler(result->server, &lora_rx_start_uri));
  httpd_uri_t lora_tx_uri = {
      .uri = "/api/v2/lora/tx",
      .method = HTTP_POST,
      .handler = at_rest_lora_tx,
      .user_ctx = result
  };
  ERROR_CHECK(httpd_register_uri_handler(result->server, &lora_tx_uri));
  httpd_uri_t fsk_tx_uri = {
      .uri = "/api/v2/fsk/tx",
      .method = HTTP_POST,
      .handler = at_rest_fsk_tx,
      .user_ctx = result
  };
  ERROR_CHECK(httpd_register_uri_handler(result->server, &fsk_tx_uri));
  httpd_uri_t fsk_rx_start_uri = {
      .uri = "/api/v2/fsk/rx/start",
      .method = HTTP_POST,
      .handler = at_rest_fsk_rx_start,
      .user_ctx = result
  };
  ERROR_CHECK(httpd_register_uri_handler(result->server, &fsk_rx_start_uri));
  httpd_uri_t rx_stop_uri = {
      .uri = "/api/v2/rx/stop",
      .method = HTTP_POST,
      .handler = at_rest_rx_stop,
      .user_ctx = result
  };
  ERROR_CHECK(httpd_register_uri_handler(result->server, &rx_stop_uri));
  httpd_uri_t rx_pull_uri = {
      .uri = "/api/v2/rx/pull",
      .method = HTTP_GET,
      .handler = at_rest_rx_pull,
      .user_ctx = result
  };
  ERROR_CHECK(httpd_register_uri_handler(result->server, &rx_pull_uri));
  httpd_uri_t status_uri = {
      .uri = "/api/v2/status",
      .method = HTTP_GET,
      .handler = at_rest_status,
      .user_ctx = result
  };
  ERROR_CHECK(httpd_register_uri_handler(result->server, &status_uri));

  *rest = result;
  return ESP_OK;
}

esp_err_t at_rest_add_frame(lora_frame_t *frame, at_rest *handler) {
  return at_util_vector_add(frame, handler->frames);
}

void at_rest_destroy(at_rest *result) {
  if (result == NULL) {
    return;
  }
  if (result->server != NULL) {
    httpd_stop(result->server);
  }
  if (result->digest != NULL) {
    free(result->digest);
  }
  free(result);
}
