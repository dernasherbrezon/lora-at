#include "ble_client.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nimble/nimble_port.h>
#include <nimble/ble.h>
#include <esp_bt.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <host/ble_gap.h>
#include <host/util/util.h>
#include <arpa/inet.h>
#include <sdkconfig.h>

#ifndef CONFIG_BLUETOOTH_CONNECTION_TIMEOUT
#define CONFIG_BLUETOOTH_CONNECTION_TIMEOUT 30000
#endif

#define MUTEX_TIMEOUT_DELTA 1000
#define BLE_ADDRESS_SIZE 6
#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) {      \
      return __err_rc;        \
    }                         \
  } while (0)

//wait a little bit longer for ble stack to process timeout and nicely return it
//won't be an issue if semaphore timeout happens first
#define WAIT_FOR_SYNC(x) \
  do {                   \
    while (client->semaphore_result == ESP_FAIL) { \
      if (xSemaphoreTake(client->semaphore, pdMS_TO_TICKS(CONFIG_BLUETOOTH_CONNECTION_TIMEOUT + MUTEX_TIMEOUT_DELTA)) == pdFALSE) { \
        ESP_LOGE(TAG, x); \
        return ESP_ERR_TIMEOUT; \
      } \
    }                         \
  } while(0)

static const char *TAG = "ble_client";

struct ble_client_t {
  uint8_t *address;
  SemaphoreHandle_t semaphore;
  esp_err_t semaphore_result;
  rx_request_t *last_request;
  bool controller_initialized;

  uint16_t conn_handle;
  bool connected;

  uint16_t start_handle;
  uint16_t end_handle;
  bool service_found;

  uint16_t request_characteristic_handle;
  bool characteristic_found;
};

// callbacks doesn't accept user's data
// so can't pass reference to the client
// thus using global client
ble_client *global_client = NULL;

//#define STATUS_UUID "5b53256e-76d2-4259-b3aa-15b5b4cfdd32"

//#define REQUEST_UUID "40d6f70c-5e28-4da4-a99e-c5298d1613fe"
static const ble_uuid_t *remote_request_chr_uuid =
    BLE_UUID128_DECLARE(0xfe, 0x13, 0x16, 0x8d, 0x29, 0xc5, 0x9e, 0xa9,
                        0xa4, 0x4d, 0x28, 0x5e, 0x0c, 0xf7, 0xd6, 0x40);

//#define SERVICE_UUID "3f5f0b4d-e311-4921-b29d-936afb8734cc"
static const ble_uuid_t *remote_svc_uuid =
    BLE_UUID128_DECLARE(0xcc, 0x34, 0x87, 0xfb, 0x6a, 0x93, 0x9d, 0xb2,
                        0x21, 0x49, 0x11, 0xe3, 0x4d, 0x0b, 0x5f, 0x3f);

// most obvious conversions
esp_err_t ble_client_convert_ble_code(int ble_code) {
  switch (ble_code) {
    case 0:
      return ESP_OK;
    case BLE_HS_ENOMEM:
      return ESP_ERR_NO_MEM;
    case BLE_HS_ENOTSUP:
      return ESP_ERR_NOT_SUPPORTED;
    case BLE_HS_ETIMEOUT:
      return ESP_ERR_TIMEOUT;
    case BLE_HS_EBADDATA:
      return ESP_ERR_INVALID_ARG;
    default:
      return ble_code;
  }
}

int ble_client_gatt_attr_fn(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  struct ble_client_t *client = (struct ble_client_t *) arg;
  ESP_LOGD(TAG, "characteristic value response received: %d status: %d", conn_handle, error->status);
  if (client->conn_handle != conn_handle) {
    return 0;
  }
  if (error->status != 0) {
    client->semaphore_result = ble_client_convert_ble_code(error->status);
    xSemaphoreGive(client->semaphore);
    return 0;
  }
  ESP_LOGI(TAG, "received bytes: %d", attr->om->om_len);
  // no observations scheduled
  if (attr->om->om_len == 0) {
    client->semaphore_result = ESP_OK;
    xSemaphoreGive(client->semaphore);
    return 0;
  }
  client->last_request = malloc(sizeof(rx_request_t));
  if (client->last_request == NULL) {
    client->semaphore_result = ESP_ERR_NO_MEM;
    xSemaphoreGive(client->semaphore);
    return 0;
  }

  if (attr->om->om_len != sizeof(rx_request_t)) {
    ESP_LOGE(TAG, "not enough bytes. expected %d", sizeof(rx_request_t));
    client->semaphore_result = ESP_ERR_NO_MEM;
    xSemaphoreGive(client->semaphore);
    return 0;
  }

  memcpy(client->last_request, attr->om->om_data, sizeof(rx_request_t));
  client->last_request->startTimeMillis = ntohll(client->last_request->startTimeMillis);
  client->last_request->endTimeMillis = ntohll(client->last_request->endTimeMillis);
  client->last_request->currentTimeMillis = ntohll(client->last_request->currentTimeMillis);
  client->last_request->freq = ntohll(client->last_request->freq);
  client->last_request->bw = ntohl(client->last_request->bw);
  client->last_request->preambleLength = ntohs(client->last_request->preambleLength);

  client->semaphore_result = ESP_OK;
  xSemaphoreGive(client->semaphore);
  return 0;
}

int ble_client_gatt_chr_fn(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
  struct ble_client_t *client = (struct ble_client_t *) arg;
  ESP_LOGD(TAG, "characteristic discovery event: %d for conn %d", error->status, conn_handle);
  if (client->conn_handle != conn_handle) {
    return 0;
  }
  switch (error->status) {
    case 0: {
      ESP_LOGI(TAG, "characteristic found");
      client->characteristic_found = true;
      client->request_characteristic_handle = chr->val_handle;
      client->semaphore_result = ESP_OK;
      xSemaphoreGive(client->semaphore);
      break;
    }
    case BLE_HS_EDONE: {
      if (!client->characteristic_found) {
        client->semaphore_result = ESP_ERR_TIMEOUT;
        xSemaphoreGive(client->semaphore);
      }
      break;
    }
  }
  return 0;
}

int ble_client_disc_svc_fn(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg) {
  struct ble_client_t *client = (struct ble_client_t *) arg;
  ESP_LOGD(TAG, "service discovery event: %d for conn %d", error->status, conn_handle);
  if (client->conn_handle != conn_handle) {
    return 0;
  }
  switch (error->status) {
    case 0: {
      ESP_LOGI(TAG, "service found");
      client->service_found = true;
      client->start_handle = service->start_handle;
      client->end_handle = service->end_handle;
      client->semaphore_result = ESP_OK;
      xSemaphoreGive(client->semaphore);
      break;
    }
    case BLE_HS_ETIMEOUT: {
      client->semaphore_result = ESP_ERR_TIMEOUT;
      xSemaphoreGive(client->semaphore);
      break;
    }
    case BLE_HS_EDONE: {
      if (!client->service_found) {
        client->semaphore_result = ESP_ERR_TIMEOUT;
        xSemaphoreGive(client->semaphore);
      }
      break;
    }
  }
  return 0;
}

static int ble_client_gap_event(struct ble_gap_event *event, void *arg) {
  ble_client *client = (ble_client *) arg;
  ESP_LOGD(TAG, "gap event: %d", event->type);
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
      int status = event->connect.status;
      if (status == 0) {
        ESP_LOGI(TAG, "connection established");
        client->conn_handle = event->connect.conn_handle;
        client->semaphore_result = ESP_OK;
        client->connected = true;
      } else {
        ESP_LOGE(TAG, "connection failed. ble code: %d", status);
        client->semaphore_result = ble_client_convert_ble_code(status);
        client->connected = false;
      }
      xSemaphoreGive(client->semaphore);
      break;
    }
    default:
      break;
  }
  return 0;
}

void ble_client_host_task(void *param) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

static void ble_client_on_reset(int reason) {
  ESP_LOGE(TAG, "resetting state. reason: %d", reason);
  global_client->connected = false;
  global_client->service_found = false;
  global_client->characteristic_found = false;
}

static void ble_client_on_sync(void) {
  esp_err_t code = ble_hs_util_ensure_addr(0);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to ensure address: %d", code);
  }
  global_client->semaphore_result = code;
  if (xSemaphoreGive(global_client->semaphore) != pdTRUE) {
    ESP_LOGE(TAG, "can't return semaphore");
  }
}

esp_err_t ble_client_create(uint8_t *address, ble_client **client) {
  if (global_client != NULL) {
    *client = global_client;
    return ESP_OK;
  }
  struct ble_client_t *result = malloc(sizeof(struct ble_client_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->semaphore = xSemaphoreCreateBinary();
  if (result->semaphore == NULL) {
    ble_client_destroy(result);
    return ESP_ERR_NO_MEM;
  }
  // pessimistic by default
  result->semaphore_result = ESP_FAIL;
  result->controller_initialized = false;
  result->connected = false;
  result->service_found = false;
  result->characteristic_found = false;
  result->last_request = NULL;
  result->address = address;

  global_client = result;
  *client = result;
  return ESP_OK;
}

esp_err_t ble_client_init_controller(ble_client *client) {
  if (client->controller_initialized) {
    return ESP_OK;
  }
  esp_err_t code = nvs_flash_init();
  if (code == ESP_ERR_NVS_NO_FREE_PAGES || code == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ERROR_CHECK(nvs_flash_erase());
    code = nvs_flash_init();
  }
  ERROR_CHECK(code);
  ble_hs_cfg.reset_cb = ble_client_on_reset;
  ble_hs_cfg.sync_cb = ble_client_on_sync;
  client->semaphore_result = ESP_FAIL;
  nimble_port_init();

  ERROR_CHECK((esp_err_t) ble_svc_gap_device_name_set(TAG));
  nimble_port_freertos_init(ble_client_host_task);
  WAIT_FOR_SYNC("timeout waiting for sync");
  client->controller_initialized = true;
  return ESP_OK;
}

esp_err_t ble_client_connect_internally(uint8_t *address, ble_client *client) {
  if (client->connected) {
    return ESP_OK;
  }
  ble_addr_t bt_address;
  bt_address.type = BLE_ADDR_PUBLIC;
  // reverse address for nimble
  for (int i = 0; i < BLE_ADDRESS_SIZE; i++) {
    bt_address.val[BLE_ADDRESS_SIZE - i - 1] = address[i];
  }
  client->semaphore_result = ESP_FAIL;
  esp_err_t code = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &bt_address, CONFIG_BLUETOOTH_CONNECTION_TIMEOUT, NULL, ble_client_gap_event, client);
  if (code != 0) {
    ESP_LOGE(TAG, "unable to connect: %d", code);
    return ESP_ERR_INVALID_ARG;
  }
  WAIT_FOR_SYNC("timeout waiting for connection");
  return client->semaphore_result;
}

esp_err_t ble_client_find_service(ble_client *client) {
  if (client->service_found) {
    return ESP_OK;
  }
  client->semaphore_result = ESP_FAIL;
  esp_err_t code = ble_gattc_disc_svc_by_uuid(client->conn_handle, remote_svc_uuid, ble_client_disc_svc_fn, client);
  if (code != 0) {
    ESP_LOGE(TAG, "unable to find service: %d", code);
    return ESP_ERR_INVALID_ARG;
  }
  WAIT_FOR_SYNC("timeout waiting for service discovery");
  return client->semaphore_result;
}

esp_err_t ble_client_find_characteristic(ble_client *client) {
  if (client->characteristic_found) {
    return ESP_OK;
  }
  client->semaphore_result = ESP_FAIL;
  int status = ble_gattc_disc_chrs_by_uuid(client->conn_handle, client->start_handle, client->end_handle, remote_request_chr_uuid, ble_client_gatt_chr_fn, client);
  if (status != 0) {
    ESP_LOGE(TAG, "unable to find characteristic: %d", status);
    return ESP_ERR_INVALID_ARG;
  }
  WAIT_FOR_SYNC("timeout waiting for characteristic discovery");
  return client->semaphore_result;
}

esp_err_t ble_client_connect(uint8_t *address, ble_client *client) {
  ERROR_CHECK(ble_client_init_controller(client));
  ERROR_CHECK(ble_client_connect_internally(address, client));
  ERROR_CHECK(ble_client_find_service(client));
  ERROR_CHECK(ble_client_find_characteristic(client));
  return ESP_OK;
}

void ble_client_disconnect(ble_client *client) {
  if (!client->connected) {
    return;
  }
  int code = ble_gap_terminate(client->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  if (code != 0) {
    ESP_LOGE(TAG, "unable to gracefully terminate connection: %d", code);
  }
  client->connected = false;
  client->service_found = false;
  client->characteristic_found = false;
}

void ble_client_log_request(rx_request_t *req) {
  char buf[80];
  struct tm *ts;
  const char *format = "%Y-%m-%d %H:%M:%S";
  uint64_t timeSeconds = req->currentTimeMillis / 1000;
  ts = localtime((const time_t *) (&timeSeconds));
  strftime(buf, sizeof(buf), format, ts);
  ESP_LOGI(TAG, "current time: %s", buf);
  timeSeconds = req->startTimeMillis / 1000;
  ts = localtime((const time_t *) (&timeSeconds));
  strftime(buf, sizeof(buf), format, ts);
  ESP_LOGI(TAG, "start time:   %s", buf);
  timeSeconds = req->endTimeMillis / 1000;
  ts = localtime((const time_t *) (&timeSeconds));
  strftime(buf, sizeof(buf), format, ts);
  ESP_LOGI(TAG, "end time:     %s", buf);
  ESP_LOGI(TAG, "observation requested: %" PRIu64 ",%" PRIu32 ",%hhu,%hhu,%hhu,%hu,%" PRIu16 ",%hhu,%hhu,%hhu,%hhu", req->freq, req->bw, req->sf, req->cr, req->syncWord, req->power, req->preambleLength, req->gain, req->ldo,
           req->useCrc, req->useExplicitHeader);
}

esp_err_t ble_client_load_request(rx_request_t **request, ble_client *client) {
  if (!client->characteristic_found) {
    ERROR_CHECK(ble_client_connect(client->address, client));
  }
  client->semaphore_result = ESP_FAIL;
  client->last_request = NULL;
  int code = ble_gattc_read(client->conn_handle, client->request_characteristic_handle, ble_client_gatt_attr_fn, client);
  if (code != 0) {
    ESP_LOGE(TAG, "unable to load request. ble code: %d", code);
    return ble_client_convert_ble_code(code);
  }
  WAIT_FOR_SYNC("timeout waiting for the data");
  if (client->semaphore_result == ESP_OK) {
    *request = client->last_request;
  }
  if (*request != NULL) {
    ble_client_log_request(*request);
  }
  return client->semaphore_result;
}

esp_err_t ble_client_send_frame(lora_frame_t *frame, ble_client *client) {
  if (!client->characteristic_found) {
    ERROR_CHECK(ble_client_connect(client->address, client));
  }
  size_t length = 0;
  length += sizeof(frame->frequency_error);
  length += sizeof(frame->rssi);
  length += sizeof(frame->snr);
  length += sizeof(frame->timestamp);
  length += sizeof(frame->data_length);
  length += frame->data_length;

  uint8_t *message = (uint8_t *) malloc(sizeof(uint8_t) * length);
  if (message == NULL) {
    return ESP_ERR_NO_MEM;
  }
  size_t offset = 0;
  int32_t frequency_error = htonl(frame->frequency_error);
  memcpy(message + offset, &frequency_error, sizeof(frequency_error));
  offset += sizeof(frequency_error);

  int16_t rssi = htons(frame->rssi);
  memcpy(message + offset, &rssi, sizeof(rssi));
  offset += sizeof(rssi);

  uint32_t snr;
  memcpy(&snr, &(frame->snr), sizeof(frame->snr));
  snr = htonl(snr);
  memcpy(message + offset, &snr, sizeof(snr));
  offset += sizeof(snr);

  uint64_t timestamp = htonll(frame->timestamp);
  memcpy(message + offset, &timestamp, sizeof(frame->timestamp));
  offset += sizeof(frame->timestamp);

  memcpy(message + offset, &frame->data_length, sizeof(frame->data_length));
  offset += sizeof(frame->data_length);

  memcpy(message + offset, frame->data, frame->data_length);
  offset += frame->data_length;

  client->semaphore_result = ESP_FAIL;
  int code = ble_gattc_write_flat(client->conn_handle, client->request_characteristic_handle, message, length, ble_client_gatt_attr_fn, client);
  if (code != 0) {
    free(message);
    ESP_LOGE(TAG, "unable to send frame. ble code: %d", code);
    return ble_client_convert_ble_code(code);
  }
  WAIT_FOR_SYNC("timeout waiting for writing");
  free(message);
  return ESP_OK;
}

void ble_client_destroy(ble_client *client) {
  if (client == NULL) {
    return;
  }
  if (client->semaphore != NULL) {
    vSemaphoreDelete(client->semaphore);
  }
  free(client);
  global_client = NULL;
}
