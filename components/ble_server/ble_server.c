#include "ble_server.h"
#include <errno.h>
#include <stdbool.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nimble/nimble_port.h>
#include <host/util/util.h>
#include <services/gap/ble_svc_gap.h>
#include <nimble/nimble_port_freertos.h>
#include "sdkconfig.h"
#include "ble_common.h"
#include "ble_solar_svc.h"
#include "ble_battery_svc.h"
#include "ble_sx127x_svc.h"

#ifndef PROJECT_VER
#define PROJECT_VER "2.0"
#endif

static const char *TAG = "ble_server";

extern void ble_server_advertise();

uint16_t ble_server_model_name_handle;
uint16_t ble_server_manuf_name_handle;
static const char ble_server_model_name[] = "lora-at";
static const char ble_server_manuf_name[] = "dernasherbrezon";
static const char ble_server_version[] = PROJECT_VER;

static int ble_server_handle_generic_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->chr == NULL) {
    return 0;
  }
  if (attr_handle == ble_server_model_name_handle) {
    ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_model_name, sizeof(ble_server_model_name)));
  }
  if (attr_handle == ble_server_manuf_name_handle) {
    ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_manuf_name, sizeof(ble_server_manuf_name)));
  }
  if (ble_uuid_cmp(ctxt->chr->uuid, BLE_UUID16_DECLARE(BLE_SERVER_SOFTWARE_VERSION_UUID)) == 0) {
    ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_version, sizeof(ble_server_version)));
  }
  return 0;
}

static const struct ble_gatt_svc_def ble_server_items[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0x92, 0xcc, 0xc7, 0x45, 0xf0, 0x72, 0x49, 0xd0, 0x92, 0xcc, 0xc7, 0x45, 0xf0, 0x72, 0x49, 0xd0),
        .characteristics = (struct ble_gatt_chr_def[])
            {{
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_MODEL_NAME_UUID),
                 .access_cb = ble_server_handle_generic_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_model_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_MANUFACTURER_NAME_UUID),
                 .access_cb = ble_server_handle_generic_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_manuf_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_SOFTWARE_VERSION_UUID),
                 .access_cb = ble_server_handle_generic_service,
                 .flags = BLE_GATT_CHR_F_READ
             },
             {
                 0
             }}
    },
    {
        0
    }
};

void ble_server_send_updates() {
  ble_solar_send_updates();
  ble_battery_send_updates();
  ble_sx127x_send_updates();
}

static int ble_server_event_handler(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      ESP_LOGI(TAG, "connection %s; conn_handle=%d status=%d ", event->connect.status == 0 ? "established" : "failed", event->connect.conn_handle, event->connect.status);
      // search first not active and take it
      for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (global_ble_server.client[i].active) {
          continue;
        }
        global_ble_server.client[i].active = true;
        global_ble_server.client[i].conn_id = event->connect.conn_handle;
        memset(global_ble_server.client[i].subsription_handles, 0, sizeof(uint16_t) * BLE_SERVER_MAX_SUBSCRIPTIONS);
        break;
      }
      ble_server_advertise();
      return 0;
    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI(TAG, "disconnect; conn_handle=%d reason=%d", event->disconnect.conn.conn_handle, event->disconnect.reason);
      for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (global_ble_server.client[i].conn_id != event->disconnect.conn.conn_handle) {
          continue;
        }
        global_ble_server.client[i].active = false;
        break;
      }
      return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
      ESP_LOGI(TAG, "connection updated; conn_handle=%d status=%d ", event->conn_update.conn_handle, event->conn_update.status);
      return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
      ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
      ble_server_advertise();
      return 0;

    case BLE_GAP_EVENT_NOTIFY_TX:
      //ESP_LOGI(GATTS_TAG, "notify_tx event; conn_handle=%d attr_handle=%d status=%d is_indication=%d", event->notify_tx.conn_handle, event->notify_tx.attr_handle, event->notify_tx.status, event->notify_tx.indication);
      return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
      ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d reason=%d prevn=%d curn=%d previ=%d curi=%d", event->subscribe.conn_handle, event->subscribe.attr_handle, event->subscribe.reason, event->subscribe.prev_notify, event->subscribe.cur_notify, event->subscribe.prev_indicate,
               event->subscribe.cur_indicate);
      for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (global_ble_server.client[i].conn_id != event->subscribe.conn_handle) {
          continue;
        }
        for (int j = 0; j < BLE_SERVER_MAX_SUBSCRIPTIONS; j++) {
          if (global_ble_server.client[i].subsription_handles[j] != 0) {
            continue;
          }
          if (event->subscribe.cur_notify > 0) {
            global_ble_server.client[i].subsription_handles[j] = event->subscribe.attr_handle;
          } else {
            global_ble_server.client[i].subsription_handles[j] = 0;
          }
          break;
        }
        break;
      }
      return 0;

    case BLE_GAP_EVENT_MTU:
      ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
      for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (global_ble_server.client[i].conn_id != event->mtu.conn_handle) {
          continue;
        }
        global_ble_server.client[i].mtu = event->mtu.value;
        break;
      }
      return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
    case BLE_GAP_EVENT_REPEAT_PAIRING:
    case BLE_GAP_EVENT_PASSKEY_ACTION:
      return 0;
  }

  return 0;
}

bool ble_server_can_accept_more() {
  for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
    if (!global_ble_server.client[i].active) {
      return true;
    }
  }
  return false;
}

void ble_server_advertise() {
  if (!ble_server_can_accept_more()) {
    return;
  }
  struct ble_gap_adv_params adv_params;
  struct ble_hs_adv_fields fields;
  const char *name;
  int rc;

  /**
   *  Set the advertisement data included in our advertisements:
   *     o Flags (indicates advertisement type and other general info).
   *     o Advertising tx power.
   *     o Device name.
   *     o 16-bit service UUIDs (alert notifications).
   */

  memset(&fields, 0, sizeof fields);

  /* Advertise two flags:
   *     o Discoverability in forthcoming advertisement (general)
   *     o BLE-only (BR/EDR unsupported).
   */
  fields.flags = BLE_HS_ADV_F_DISC_GEN |
                 BLE_HS_ADV_F_BREDR_UNSUP;

  /* Indicate that the TX power level field should be included; have the
   * stack fill this value automatically.  This is done by assigning the
   * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
   */
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  name = ble_svc_gap_device_name();
  fields.name = (uint8_t *) name;
  fields.name_len = strlen(name);
  fields.name_is_complete = 1;
  fields.uuids16 = NULL;
  fields.num_uuids16 = 0;
  fields.uuids16_is_complete = 1;

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
    return;
  }

  /* Begin advertising. */
  memset(&adv_params, 0, sizeof adv_params);
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, ble_server_event_handler, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
    return;
  }
}

static void ble_client_on_reset(int reason) {
  ESP_LOGE(TAG, "resetting state. reason: %d", reason);
}

static void ble_client_on_sync(void) {
  esp_err_t code = ble_hs_util_ensure_addr(0);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to ensure address: %d", code);
    return;
  }
  uint8_t addr_val[6] = {0};
  code = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, addr_val, NULL);
  if (code != ESP_OK) {
    ESP_LOGE(TAG, "unable to get own address: %d", code);
    return;
  }
  char device_name[8 + 4 + 1];
  uint16_t unique = 0;
  for (int i = 0; i < 6; i++) {
    unique += addr_val[i];
  }
  snprintf(device_name, sizeof(device_name), "lora-at-%04x", unique);
  ESP_LOGI(TAG, "device name: %s", device_name);
  ESP_ERROR_CHECK((esp_err_t) ble_svc_gap_device_name_set(device_name));

  ble_server_advertise();
}

void ble_server_host_task(void *param) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

esp_err_t ble_server_create(at_sensors *sensors, sx127x *device) {
  global_ble_server.sensors = sensors;
  global_ble_server.device = device;

  // Initialize NVS.
  esp_err_t code = nvs_flash_init();
  if (code == ESP_ERR_NVS_NO_FREE_PAGES || code == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    code = nvs_flash_init();
  }
  ESP_ERROR_CHECK(code);

  ble_hs_cfg.reset_cb = ble_client_on_reset;
  ble_hs_cfg.sync_cb = ble_client_on_sync;

  ERROR_CHECK(nimble_port_init());
  ERROR_CHECK(ble_gatts_count_cfg(ble_server_items));
  ERROR_CHECK(ble_gatts_add_svcs(ble_server_items));
  ERROR_CHECK(ble_solar_svc_register());
  ERROR_CHECK(ble_battery_svc_register());
  ERROR_CHECK(ble_sx127x_svc_register());

  nimble_port_freertos_init(ble_server_host_task);
  return ESP_OK;
}
