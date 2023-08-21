#include "ble_client.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nimble/nimble_port.h>
#include <esp_bt.h>
#include <nimble/nimble_port_freertos.h>
#include <esp_nimble_hci.h>
#include <services/gap/ble_svc_gap.h>
#include <host/ble_gap.h>
#include <host/util/util.h>

#define CONNECTION_TIMEOUT 30000
#define ERROR_CHECK(x)        \
  do {                        \
    esp_err_t __err_rc = (x); \
    if (__err_rc != 0) {      \
      global_client = NULL;          \
      return __err_rc;        \
    }                         \
  } while (0)

static const char *TAG = "lora-at";

//#define REQUEST_UUID "40d6f70c-5e28-4da4-a99e-c5298d1613fe"
//#define STATUS_UUID "5b53256e-76d2-4259-b3aa-15b5b4cfdd32"

//#define SERVICE_UUID "3f5f0b4d-e311-4921-b29d-936afb8734cc"
static const ble_uuid_t *remote_svc_uuid =
    BLE_UUID128_DECLARE(0xcc, 0x34, 0x87, 0xfb, 0x6a, 0x93, 0x9d, 0xb2,
                        0x21, 0x49, 0x11, 0xe3, 0x4d, 0x0b, 0x5f, 0x3f);

// callbacks doesn't accept user's data
// so can't pass reference to the client
// thus using global client
static ble_client_t *global_client = NULL;

//void ble_client_esp_gap_ble_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
//  //FIXME
//}
//
//void ble_client_esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
//  if (global_client == NULL) {
//    return;
//  }
//  switch (event) {
//    case ESP_GATTC_REG_EVT: {
//      if (param->reg.status != ESP_GATT_OK) {
//        ESP_LOGE(TAG, "unable to register: %d", param->reg.status);
//        break;
//      }
//      if (param->reg.app_id != global_client->app_id) {
//        break;
//      }
//      global_client->gatt_if = gattc_if;
//      break;
//    }
//    case ESP_GATTC_CONNECT_EVT: {
//      global_client->conn_id = param->connect.conn_id;
//      esp_err_t code = esp_ble_gattc_send_mtu_req(global_client->gatt_if, global_client->conn_id);
//      if (code != ESP_OK && global_client->verify_callback != NULL) {
//        global_client->verify_callback(code, global_client->user_data);
//      }
//      break;
//    }
//    case ESP_GATTC_OPEN_EVT: {
//      if (param->open.conn_id != global_client->conn_id) {
//        break;
//      }
//      if (param->open.status != ESP_GATT_OK) {
//        ESP_LOGE(TAG, "unable to open connection: %d", param->open.status);
//        if (global_client->verify_callback != NULL) {
//          global_client->verify_callback(ESP_ERR_INVALID_RESPONSE, global_client->user_data);
//        }
//      }
//      break;
//    }
//    case ESP_GATTC_CFG_MTU_EVT: {
//      esp_err_t code = esp_ble_gattc_search_service(global_client->gatt_if, global_client->conn_id, &service_uuid);
//      if (code != ESP_OK && global_client->verify_callback != NULL) {
//        global_client->verify_callback(code, global_client->user_data);
//      }
//      break;
//    }
//    case ESP_GATTC_SEARCH_RES_EVT: {
//      if (global_client->conn_id != param->search_res.conn_id) {
//        break;
//      }
//      global_client->found_service = true;
//      global_client->service_start_handle = param->search_res.start_handle;
//      global_client->service_end_handle = param->search_res.end_handle;
//      break;
//    }
//    case ESP_GATTC_SEARCH_CMPL_EVT: {
//      if (param->search_cmpl.conn_id != global_client->conn_id) {
//        break;
//      }
//      if (param->search_cmpl.status != ESP_GATT_OK) {
//        ESP_LOGE(TAG, "unable to find service: %d", param->search_cmpl.status);
//        if (global_client->verify_callback != NULL) {
//          global_client->verify_callback(ESP_ERR_INVALID_RESPONSE, global_client->user_data);
//        }
//        break;
//      }
//      if (global_client->verify_callback != NULL) {
//        if (global_client->found_service) {
//          global_client->verify_callback(ESP_OK, global_client->user_data);
//        } else {
//          global_client->verify_callback(ESP_ERR_NOT_FOUND, global_client->user_data);
//        }
//      }
//      break;
//    }
//    default:
//      break;
//  }
//}

int ble_client_disc_svc_fn(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *service,
                           void *arg) {
  ble_client_t *client = (ble_client_t *) arg;
  if (client->conn_handle != conn_handle) {
    return 0;
  }
  esp_err_t code;
  switch (error->status) {
    case 0: {
      client->start_handle = service->start_handle;
      client->end_handle = service->end_handle;
      client->service_found = true;
      code = ESP_OK;
      client->semaphore_result = code;
      xSemaphoreGive(client->semaphore);
      break;
    }
    case BLE_HS_EDONE: {
      if (!client->service_found) {
        code = ESP_ERR_TIMEOUT;
      } else {
        code = ESP_OK;
      }
      break;
    }
    default:
      code = ESP_ERR_INVALID_STATE;
      break;
  }
  if (code != ESP_OK) {
    client->semaphore_result = code;
    xSemaphoreGive(client->semaphore);
  }
  return 0;
}

static int ble_client_gap_event(struct ble_gap_event *event, void *arg) {
  ble_client_t *client = (ble_client_t *) arg;
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
      int status = event->connect.status;
      if (status == 0) {
        client->conn_handle = event->connect.conn_handle;
        client->service_found = false;
        status = ble_gattc_disc_svc_by_uuid(client->conn_handle, remote_svc_uuid, ble_client_disc_svc_fn, client);
      }
      if (status != 0) {
        ESP_LOGE(TAG, "connection failed: %d", status);
        client->semaphore_result = ESP_ERR_INVALID_STATE;
        xSemaphoreGive(client->semaphore);
      }
      break;
    }
    default:
      break;
  }
  return 0;
}

void ble_client_host_task(void *param) {
  nimble_port_run();
}

static void ble_client_on_reset(int reason) {
  ESP_LOGE(TAG, "resetting state. reason: %d", reason);
}

static void ble_client_on_sync(void) {
  int code = ble_hs_util_ensure_addr(0);
  if (code != 0) {
    ESP_LOGE(TAG, "unable to ensure address: %d", code);
  }
}

esp_err_t ble_client_create(uint16_t app_id, ble_client_t **client) {
  if (global_client != NULL) {
    *client = global_client;
    return ESP_OK;
  }
  ble_client_t *result = malloc(sizeof(ble_client_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result->semaphore = xSemaphoreCreateMutex();
  // pessimistic by default
  result->semaphore_result = ESP_ERR_INVALID_ARG;
  ERROR_CHECK(nvs_flash_init());
  nimble_port_init();

  ble_hs_cfg.reset_cb = ble_client_on_reset;
  ble_hs_cfg.sync_cb = ble_client_on_sync;

  ERROR_CHECK((esp_err_t) ble_svc_gap_device_name_set(TAG));
  nimble_port_freertos_init(ble_client_host_task);

  global_client = result;
  *client = result;
  return ESP_OK;
}

esp_err_t ble_client_connect(ble_addr_t *bt_address, ble_client_t *client) {
  client->semaphore_result = ESP_ERR_INVALID_ARG;
  int code = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, bt_address, CONNECTION_TIMEOUT, NULL, ble_client_gap_event, client);
  if (code != 0) {
    ESP_LOGE(TAG, "unable to connect: %d", code);
    return ESP_ERR_INVALID_ARG;
  }
  if (xSemaphoreTake(client->semaphore, pdMS_TO_TICKS(CONNECTION_TIMEOUT)) == pdFALSE) {
    ESP_LOGE(TAG, "timeout waiting for connection");
    return ESP_ERR_TIMEOUT;
  }
  return client->semaphore_result;
}

void ble_client_destroy(ble_client_t *client) {
  if (client == NULL) {
    return;
  }
  nimble_port_freertos_deinit();
  nimble_port_deinit();
  free(client);
  global_client = NULL;
}
