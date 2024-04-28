#include <host/ble_gatt.h>
#include <os/os_mbuf.h>
#include <rom/ets_sys.h>
#include <esp_log.h>
#include <host/ble_gap.h>
#include "ble_sx127x_svc.h"
#include "ble_common.h"
#include "sdkconfig.h"
#include "sx127x_util.h"

static const char *SX127X_SVC_TAG = "sx127x_svc";

#ifndef CONFIG_AT_SX127X_TEMPERATURE_CORRECTION
#define CONFIG_AT_SX127X_TEMPERATURE_CORRECTION 0
#endif

uint16_t ble_server_sx127x_temperature_handle;
uint16_t ble_server_sx127x_startrx_handle;
uint16_t ble_server_sx127x_stoprx_handle;

static int ble_server_handle_sx127x_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    if (attr_handle == ble_server_sx127x_temperature_handle) {
      int8_t temperature;
      ESP_ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_FSRX, SX127x_MODULATION_FSK, global_ble_server.device));
      //enable temp monitoring
      ESP_ERROR_CHECK(sx127x_fsk_ook_set_temp_monitor(true, global_ble_server.device));
      // a little bit longer for FSRX mode to kick off
      ets_delay_us(150);
      //disable temp monitoring
      ESP_ERROR_CHECK(sx127x_fsk_ook_set_temp_monitor(false, global_ble_server.device));
      ESP_ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_FSK, global_ble_server.device));

      ERROR_CHECK_CALLBACK(sx127x_fsk_ook_get_raw_temperature(global_ble_server.device, &temperature));
      temperature += CONFIG_AT_SX127X_TEMPERATURE_CORRECTION;
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &temperature, sizeof(temperature)));
    }
  }
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    if (!ble_server_is_authorized(conn_handle)) {
      ESP_LOGI(SX127X_SVC_TAG, "connection %d is not authorized", conn_handle);
      return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
    }
    if (attr_handle == ble_server_sx127x_startrx_handle) {
      lora_config_t lora_req;
      ERROR_CHECK_CALLBACK(os_mbuf_copydata(ctxt->om, 0, sizeof(lora_config_t), &lora_req));
      ERROR_CHECK_RESPONSE(sx127x_util_lora_rx(SX127x_MODE_RX_CONT, &lora_req, global_ble_server.device));
    }
    if (attr_handle == ble_server_sx127x_stoprx_handle) {
      ERROR_CHECK_RESPONSE(sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, global_ble_server.device));
    }
  }
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
    if (ctxt->dsc != NULL) {
      if (attr_handle == ble_server_sx127x_temperature_handle && ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT)) == 0) {
        ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &celsius_format, sizeof(celsius_format)));
      }
    }
  }
  return 0;
}

static const struct ble_gatt_svc_def ble_sx127x_items[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0xe6, 0xcf, 0xb1, 0xb8, 0x47, 0x87, 0x46, 0x48, 0xaf, 0x27, 0x2, 0x6c, 0xc2, 0x2d, 0xf9, 0x5f),
        .characteristics = (struct ble_gatt_chr_def[])
            {{
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_TEMPERATURE_UUID),
                 .access_cb = ble_server_handle_sx127x_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_sx127x_temperature_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_sx127x_service,
                      },
                      {
                          0
                      }}
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0xab, 0x50, 0x9d, 0xf5, 0x1, 0xdb, 0x40, 0x1d, 0x9f, 0xde, 0xa9, 0x4e, 0xd7, 0x53, 0x20, 0xbb),
                 .access_cb = ble_server_handle_sx127x_service,
                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                 .val_handle = &ble_server_sx127x_startrx_handle
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0x36, 0x40, 0xdd, 0xa2, 0x77, 0xa9, 0x41, 0x68, 0xa4, 0x83, 0x8e, 0xab, 0x3, 0xa, 0x2b, 0x89),
                 .access_cb = ble_server_handle_sx127x_service,
                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                 .val_handle = &ble_server_sx127x_stoprx_handle
             },
             {
                 0
             }}
    },
    {
        0
    }
};

void ble_sx127x_send_updates() {
  int8_t temperature;
  esp_err_t temperature_code = ESP_ERR_NOT_SUPPORTED;
  if (ble_server_has_subscription(ble_server_sx127x_temperature_handle)) {
    temperature_code = sx127x_fsk_ook_get_raw_temperature(global_ble_server.device, &temperature);
    temperature += CONFIG_AT_SX127X_TEMPERATURE_CORRECTION;
  }
  if (temperature_code == ESP_OK) {
    ble_solar_send_update(ble_server_sx127x_temperature_handle, &temperature, sizeof(temperature));
  }
}

void ble_sx127x_send_frame(lora_frame_t *frame) {

}

esp_err_t ble_sx127x_svc_register() {
  ERROR_CHECK(ble_gatts_count_cfg(ble_sx127x_items));
  ERROR_CHECK(ble_gatts_add_svcs(ble_sx127x_items));
  return ESP_OK;
}