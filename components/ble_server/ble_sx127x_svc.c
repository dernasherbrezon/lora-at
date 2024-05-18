#include <host/ble_gatt.h>
#include <os/os_mbuf.h>
#include <rom/ets_sys.h>
#include <esp_log.h>
#include <host/ble_gap.h>
#include <cc.h>
#include <sys/time.h>
#include "ble_sx127x_svc.h"
#include "ble_common.h"
#include "sdkconfig.h"
#include "sx127x_util.h"

#define PROTOCOL_VERSION 2

static const char *SX127X_SVC_TAG = "sx127x_svc";

#ifndef CONFIG_AT_SX127X_TEMPERATURE_CORRECTION
#define CONFIG_AT_SX127X_TEMPERATURE_CORRECTION 0
#endif

uint16_t ble_server_sx127x_startrx_handle;
uint16_t ble_server_sx127x_stoprx_handle;
uint16_t ble_server_sx127x_frame_handle;

static int ble_server_handle_sx127x_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    if (!ble_server_is_authorized(conn_handle)) {
      ESP_LOGI(SX127X_SVC_TAG, "connection %d is not authorized", conn_handle);
      return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
    }
    if (attr_handle == ble_server_sx127x_startrx_handle) {
      lora_config_t lora_req;
      if (OS_MBUF_PKTLEN(ctxt->om) < sizeof(lora_config_t)) {
        ESP_LOGI("ble", "invalid request");
        return BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      ERROR_CHECK_CALLBACK(os_mbuf_copydata(ctxt->om, 0, sizeof(lora_config_t), &lora_req));
      lora_req.startTimeMillis = ntohll(lora_req.startTimeMillis);
      lora_req.endTimeMillis = ntohll(lora_req.endTimeMillis);
      lora_req.currentTimeMillis = ntohll(lora_req.currentTimeMillis);
      lora_req.freq = ntohll(lora_req.freq);
      lora_req.bw = ntohl(lora_req.bw);
      lora_req.preambleLength = ntohs(lora_req.preambleLength);
      sx127x_util_log_request(&lora_req);
      // stop rx in case BLE disconnected and stoprx was missed
      sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, global_ble_server.device->device);
      // sync time with the client
      // time will be used in rx callback for precise beacon reception
      struct timeval tm_vl;
      tm_vl.tv_sec = lora_req.currentTimeMillis / 1000;
      tm_vl.tv_usec = 0;
      settimeofday(&tm_vl, NULL);
      ERROR_CHECK_RESPONSE(sx127x_util_lora_rx(SX127x_MODE_RX_CONT, &lora_req, global_ble_server.device));
    }
    if (attr_handle == ble_server_sx127x_stoprx_handle) {
      ERROR_CHECK_RESPONSE(sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, global_ble_server.device->device));
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
                 .uuid = BLE_UUID128_DECLARE(0x72, 0xca, 0xaf, 0x36, 0x4d, 0x63, 0x43, 0xd6, 0xa8, 0xe6, 0x7f, 0x42, 0x1b, 0xae, 0x37, 0x3c),
                 .access_cb = ble_server_handle_sx127x_service,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &ble_server_sx127x_frame_handle,
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0xab, 0x50, 0x9d, 0xf5, 0x1, 0xdb, 0x40, 0x1d, 0x9f, 0xde, 0xa9, 0x4e, 0xd7, 0x53, 0x20, 0xbb),
                 .access_cb = ble_server_handle_sx127x_service,
                 .flags = BLE_GATT_CHR_PROP_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                 .val_handle = &ble_server_sx127x_startrx_handle
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0x36, 0x40, 0xdd, 0xa2, 0x77, 0xa9, 0x41, 0x68, 0xa4, 0x83, 0x8e, 0xab, 0x3, 0xa, 0x2b, 0x89),
                 .access_cb = ble_server_handle_sx127x_service,
                 .flags = BLE_GATT_CHR_PROP_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
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

void ble_sx127x_send_frame(sx127x_frame_t *frame) {
  size_t length = 0;
  length += sizeof(uint8_t);
  length += sizeof(frame->frequency_error);
  length += sizeof(frame->rssi);
  length += sizeof(frame->snr);
  length += sizeof(frame->timestamp);
  length += sizeof(frame->data_length);
  length += frame->data_length;

  uint8_t *message = (uint8_t *) malloc(sizeof(uint8_t) * length);
  if (message == NULL) {
    return;
  }
  size_t offset = 0;
  uint8_t protocol_version = PROTOCOL_VERSION;
  memcpy(message + offset, &protocol_version, sizeof(uint8_t));
  offset += sizeof(uint8_t);
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
  uint16_t data_length_network_order = htons(frame->data_length);
  memcpy(message + offset, &data_length_network_order, sizeof(frame->data_length));
  offset += sizeof(frame->data_length);
  memcpy(message + offset, frame->data, frame->data_length);

  ble_server_send_update(ble_server_sx127x_frame_handle, message, length);
  free(message);
}

esp_err_t ble_sx127x_svc_register() {
  ERROR_CHECK(ble_gatts_count_cfg(ble_sx127x_items));
  ERROR_CHECK(ble_gatts_add_svcs(ble_sx127x_items));
  return ESP_OK;
}