#include "ble_server.h"
#include <errno.h>
#include <stdbool.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <at_sensors.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nimble/nimble_port.h>
#include <host/util/util.h>
#include <services/gap/ble_svc_gap.h>
#include <nimble/nimble_port_freertos.h>
#include <cc.h>
#include "sdkconfig.h"

#ifndef PROJECT_VER
#define PROJECT_VER "2.0"
#endif

#ifndef CONFIG_AT_BATTERY_MODEL
#define CONFIG_AT_BATTERY_MODEL "Unknown"
#endif

#ifndef CONFIG_AT_BATTERY_VENDOR
#define CONFIG_AT_BATTERY_VENDOR "Unknown"
#endif

#ifndef CONFIG_AT_SOLAR_MODEL
#define CONFIG_AT_SOLAR_MODEL "Unknown"
#endif

#ifndef CONFIG_AT_SOLAR_VENDOR
#define CONFIG_AT_SOLAR_VENDOR "Unknown"
#endif

#define ERROR_CHECK(x)        \
  do {                        \
    int __err_rc = (x); \
    if (__err_rc != 0) {      \
      return __err_rc;        \
    }                         \
  } while (0)

typedef enum {
  BATTERY_LEVEL_NOTIFICATION = 0,
  SOLAR_CURRENT_NOTIFICATION = 1,
  SOLAR_VOLTAGE_NOTIFICATION = 2,
  NOTIFICATION_TOTAL = 3
} ble_server_notification_type_t;

typedef struct {
  bool active;
  uint16_t conn_id;
  bool notifications[NOTIFICATION_TOTAL];
  uint16_t mtu;
} ble_server_client_t;

typedef struct __attribute__((packed)) {
  uint8_t gatt_format;
  int8_t exponent;
  uint16_t gatt_unit;
  uint8_t gatt_namespace;
  uint16_t gatt_nsdesc;
} ble_presentation_format_t;

struct ble_server_t {
  uint8_t temp_buffer[CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU];
  at_sensors *sensors;
  ble_server_client_t client[CONFIG_BT_NIMBLE_MAX_CONNECTIONS];
};

static const ble_presentation_format_t voltage_format = {
    .gatt_format = 0x06, //uint16
    .exponent = 0x03,
    .gatt_unit = 0x2728, //volt
    .gatt_namespace = 0x00,
    .gatt_nsdesc = 0x0000
};

static const ble_presentation_format_t current_format = {
    .gatt_format = 0x06, //uint16
    .exponent = 0x03,
    .gatt_unit = 0x2704, //ampere
    .gatt_namespace = 0x00,
    .gatt_nsdesc = 0x0000
};

static const char *TAG = "ble_server";

extern void ble_server_advertise();

uint16_t ble_server_model_name_handle;
uint16_t ble_server_manuf_name_handle;
static const char ble_server_model_name[] = "lora-at";
static const char ble_server_manuf_name[] = "dernasherbrezon";
static const char ble_server_version[] = PROJECT_VER;

uint16_t ble_server_battery_model_name_handle;
uint16_t ble_server_battery_manuf_name_handle;
uint16_t ble_server_battery_level_handle;
static const char ble_server_battery_model_name[] = CONFIG_AT_BATTERY_MODEL;
static const char ble_server_battery_manuf_name[] = CONFIG_AT_BATTERY_VENDOR;

uint16_t ble_server_solar_model_name_handle;
uint16_t ble_server_solar_manuf_name_handle;
uint16_t ble_server_solar_voltage_handle;
uint16_t ble_server_solar_current_handle;
static const char ble_server_solar_model_name[] = CONFIG_AT_SOLAR_MODEL;
static const char ble_server_solar_manuf_name[] = CONFIG_AT_SOLAR_VENDOR;

ble_server *global_ble_server = NULL;

static int ble_server_handle_generic_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->chr == NULL) {
    return 0;
  }
  if (attr_handle == ble_server_model_name_handle) {
    int rc = os_mbuf_append(ctxt->om, &ble_server_model_name, sizeof(ble_server_model_name));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (attr_handle == ble_server_manuf_name_handle) {
    int rc = os_mbuf_append(ctxt->om, &ble_server_manuf_name, sizeof(ble_server_manuf_name));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (ble_uuid_cmp(ctxt->chr->uuid, BLE_UUID16_DECLARE(0x2A28)) == 0) {
    int rc = os_mbuf_append(ctxt->om, &ble_server_version, sizeof(ble_server_version));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return 0;
}

static int ble_server_handle_battery_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->chr == NULL) {
    return 0;
  }
  if (attr_handle == ble_server_battery_model_name_handle) {
    int rc = os_mbuf_append(ctxt->om, &ble_server_battery_model_name, sizeof(ble_server_battery_model_name));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (attr_handle == ble_server_battery_manuf_name_handle) {
    int rc = os_mbuf_append(ctxt->om, &ble_server_battery_manuf_name, sizeof(ble_server_battery_manuf_name));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (attr_handle == ble_server_battery_level_handle) {
    uint8_t level;
    esp_err_t code = at_sensors_get_battery_level(&level, global_ble_server->sensors);
    if (code != ESP_OK) {
      return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    int rc = os_mbuf_append(ctxt->om, &level, sizeof(level));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return 0;
}

static int ble_server_handle_solar_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    if (attr_handle == ble_server_solar_model_name_handle) {
      int rc = os_mbuf_append(ctxt->om, &ble_server_solar_model_name, sizeof(ble_server_solar_model_name));
      return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (attr_handle == ble_server_solar_manuf_name_handle) {
      int rc = os_mbuf_append(ctxt->om, &ble_server_solar_manuf_name, sizeof(ble_server_solar_manuf_name));
      return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctxt->chr != NULL) {
      if (ble_uuid_cmp(ctxt->chr->uuid, BLE_UUID16_DECLARE(0x2B18)) == 0) {
        uint16_t voltage;
        esp_err_t code = at_sensors_get_solar_voltage(&voltage, global_ble_server->sensors);
        if (code != ESP_OK) {
          return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        voltage = htons(voltage);
        int rc = os_mbuf_append(ctxt->om, &voltage, sizeof(voltage));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      if (ble_uuid_cmp(ctxt->chr->uuid, BLE_UUID16_DECLARE(0x2AEE)) == 0) {
        int16_t current;
        esp_err_t code = at_sensors_get_solar_current(&current, global_ble_server->sensors);
        if (code != ESP_OK) {
          return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        current = htons(current);
        int rc = os_mbuf_append(ctxt->om, &current, sizeof(current));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
    }
  }
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
    if (ctxt->dsc != NULL) {
      if (attr_handle == ble_server_solar_voltage_handle) {
        int rc = os_mbuf_append(ctxt->om, &voltage_format, sizeof(voltage_format));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      if (attr_handle == ble_server_solar_current_handle) {
        int rc = os_mbuf_append(ctxt->om, &current_format, sizeof(current_format));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
    }
  }
  return 0;
}

static const struct ble_gatt_svc_def ble_server_items[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0x92, 0xcc, 0xc7, 0x45, 0xf0, 0x72, 0x49, 0xd0, 0x92, 0xcc, 0xc7, 0x45, 0xf0, 0x72, 0x49, 0xd0),
        .characteristics = (struct ble_gatt_chr_def[])
            {{
                 .uuid = BLE_UUID16_DECLARE(0x2A24),
                 .access_cb = ble_server_handle_generic_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_model_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(0x2A29),
                 .access_cb = ble_server_handle_generic_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_manuf_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(0x2A28),
                 .access_cb = ble_server_handle_generic_service,
                 .flags = BLE_GATT_CHR_F_READ
             },
             {
                 0
             }}
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180F),
        .characteristics = (struct ble_gatt_chr_def[])
            {{
                 .uuid = BLE_UUID16_DECLARE(0x2A24),
                 .access_cb = ble_server_handle_battery_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_battery_model_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(0x2A29),
                 .access_cb = ble_server_handle_battery_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_battery_manuf_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(0x2A19),
                 .access_cb = ble_server_handle_battery_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_battery_level_handle
             },
             {
                 0
             }}
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0x8d, 0xe1, 0x2b, 0x32, 0x41, 0xe, 0x45, 0xf9, 0xaa, 0x6d, 0x2f, 0xec, 0x81, 0xb9, 0xa5, 0x95),
        .characteristics = (struct ble_gatt_chr_def[])
            {{
                 .uuid = BLE_UUID16_DECLARE(0x2A24),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_solar_model_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(0x2A29),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_solar_manuf_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(0x2B18),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_solar_voltage_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(0x2904),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_solar_service,
                      },
                      {
                          0
                      }}
             },
             {
                 .uuid = BLE_UUID16_DECLARE(0x2AEE),
                 .access_cb = ble_server_handle_solar_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_solar_current_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(0x2904),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_solar_service,
                      },
                      {
                          0
                      }}
             },
             {
                 0
             }}
    },
    {
        0
    }
};

bool ble_server_has_notification(ble_server_notification_type_t type) {
  for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
    if (!global_ble_server->client[i].active) {
      continue;
    }
    if (global_ble_server->client[i].notifications[type]) {
      return true;
    }
  }
  return false;
}

void ble_server_send_notifications(ble_server *server) {
  uint8_t battery_level;
  esp_err_t batter_level_code = ESP_ERR_NOT_SUPPORTED;
  if (ble_server_has_notification(BATTERY_LEVEL_NOTIFICATION)) {
    batter_level_code = at_sensors_get_battery_level(&battery_level, global_ble_server->sensors);
  }
  uint16_t solar_voltage;
  esp_err_t solar_voltage_code = ESP_ERR_NOT_SUPPORTED;
  if (ble_server_has_notification(SOLAR_VOLTAGE_NOTIFICATION)) {
    solar_voltage_code = at_sensors_get_solar_voltage(&solar_voltage, global_ble_server->sensors);
  }
  int16_t solar_current;
  esp_err_t solar_current_code = ESP_ERR_NOT_SUPPORTED;
  if (ble_server_has_notification(SOLAR_CURRENT_NOTIFICATION)) {
    solar_current_code = at_sensors_get_solar_current(&solar_current, global_ble_server->sensors);
  }

  for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
    if (!global_ble_server->client[i].active) {
      continue;
    }
    if (batter_level_code == ESP_OK && global_ble_server->client[i].notifications[BATTERY_LEVEL_NOTIFICATION]) {
      memcpy(global_ble_server->temp_buffer, &battery_level, sizeof(battery_level));
      struct os_mbuf *txom = ble_hs_mbuf_from_flat(global_ble_server->temp_buffer, sizeof(battery_level));
      ble_gatts_notify_custom(global_ble_server->client[i].conn_id, ble_server_battery_level_handle, txom);
    }
    if (solar_voltage_code == ESP_OK && global_ble_server->client[i].notifications[SOLAR_VOLTAGE_NOTIFICATION]) {
      memcpy(global_ble_server->temp_buffer, &solar_voltage, sizeof(solar_voltage));
      struct os_mbuf *txom = ble_hs_mbuf_from_flat(global_ble_server->temp_buffer, sizeof(solar_voltage));
      ble_gatts_notify_custom(global_ble_server->client[i].conn_id, ble_server_solar_voltage_handle, txom);
    }
    if (solar_current_code == ESP_OK && global_ble_server->client[i].notifications[SOLAR_CURRENT_NOTIFICATION]) {
      memcpy(global_ble_server->temp_buffer, &solar_current, sizeof(solar_current));
      struct os_mbuf *txom = ble_hs_mbuf_from_flat(global_ble_server->temp_buffer, sizeof(solar_current));
      ble_gatts_notify_custom(global_ble_server->client[i].conn_id, ble_server_solar_current_handle, txom);
    }
  }
}

static ble_server_notification_type_t ble_server_get_notify_type(uint16_t handle) {
  if (handle == ble_server_solar_current_handle) {
    return SOLAR_CURRENT_NOTIFICATION;
  }
  if (handle == ble_server_solar_voltage_handle) {
    return SOLAR_VOLTAGE_NOTIFICATION;
  }
  if (handle == ble_server_battery_level_handle) {
    return BATTERY_LEVEL_NOTIFICATION;
  }
  return 0;
}

static int ble_server_event_handler(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      ESP_LOGI(TAG, "connection %s; conn_handle=%d status=%d ", event->connect.status == 0 ? "established" : "failed", event->connect.conn_handle, event->connect.status);
      // search first not active and take it
      for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (global_ble_server->client[i].active) {
          continue;
        }
        global_ble_server->client[i].active = true;
        global_ble_server->client[i].conn_id = event->connect.conn_handle;
        break;
      }
      ble_server_advertise();
      return 0;
    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI(TAG, "disconnect; conn_handle=%d reason=%d", event->disconnect.conn.conn_handle, event->disconnect.reason);
      for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (global_ble_server->client[i].conn_id != event->disconnect.conn.conn_handle) {
          continue;
        }
        global_ble_server->client[i].active = false;
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
        if (global_ble_server->client[i].conn_id != event->subscribe.conn_handle) {
          continue;
        }
        global_ble_server->client[i].notifications[ble_server_get_notify_type(event->subscribe.attr_handle)] = (event->subscribe.cur_notify > 0);
        break;
      }
      return 0;

    case BLE_GAP_EVENT_MTU:
      ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
      for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (global_ble_server->client[i].conn_id != event->mtu.conn_handle) {
          continue;
        }
        global_ble_server->client[i].mtu = event->mtu.value;
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

void ble_server_advertise() {
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

esp_err_t ble_server_create(at_sensors *sensors, ble_server **server) {
  struct ble_server_t *result = malloc(sizeof(struct ble_server_t));
  if (result == NULL) {
    return ESP_ERR_NO_MEM;
  }
  memset(result, 0, sizeof(struct ble_server_t));
  result->sensors = sensors;

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

  nimble_port_freertos_init(ble_server_host_task);
  global_ble_server = result;
  *server = result;
  return ESP_OK;
}

void ble_server_destroy(ble_server *server) {
  if (server == NULL) {
    return;
  }
  free(server);
}