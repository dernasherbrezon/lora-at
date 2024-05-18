#include "ble_antenna_svc.h"
#include "ble_common.h"
#include <cc.h>
#include <host/ble_gatt.h>
#include <os/os_mbuf.h>
#include <host/ble_hs_mbuf.h>
#include "sdkconfig.h"

#ifndef CONFIG_AT_ANTENNA_MODEL
#define CONFIG_AT_ANTENNA_MODEL "Unknown"
#endif

#ifndef CONFIG_AT_ANTENNA_VENDOR
#define CONFIG_AT_ANTENNA_VENDOR "Unknown"
#endif

#ifndef CONFIG_AT_ANTENNA_TYPE
#define CONFIG_AT_ANTENNA_TYPE "Unknown"
#endif

#ifndef CONFIG_AT_ANTENNA_POLARIZATION
#define CONFIG_AT_ANTENNA_POLARIZATION "Unknown"
#endif

#ifndef CONFIG_AT_ANTENNA_FREQ_RANGE
#define CONFIG_AT_ANTENNA_FREQ_RANGE "Unknown"
#endif

uint16_t ble_server_antenna_model_name_handle;
uint16_t ble_server_antenna_manuf_name_handle;
uint16_t ble_server_antenna_type_handle;
uint16_t ble_server_antenna_polarization_handle;
uint16_t ble_server_antenna_freq_range_handle;

static const char ble_server_antenna_model_name[] = CONFIG_AT_ANTENNA_MODEL;
static const char ble_server_antenna_manuf_name[] = CONFIG_AT_ANTENNA_VENDOR;
static const char ble_server_antenna_type[] = "Type";
static const char ble_server_antenna_type_name[] = CONFIG_AT_ANTENNA_TYPE;
static const char ble_server_antenna_polarization[] = "Polarization";
static const char ble_server_antenna_polarization_name[] = CONFIG_AT_ANTENNA_POLARIZATION;
static const char ble_server_antenna_freq_range[] = "Freq range";
static const char ble_server_antenna_freq_range_name[] = CONFIG_AT_ANTENNA_FREQ_RANGE;

static int ble_server_handle_antenna_service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    if (attr_handle == ble_server_antenna_model_name_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_antenna_model_name, sizeof(ble_server_antenna_model_name)));
    }
    if (attr_handle == ble_server_antenna_manuf_name_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_antenna_manuf_name, sizeof(ble_server_antenna_manuf_name)));
    }
    if (attr_handle == ble_server_antenna_type_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_antenna_type_name, sizeof(ble_server_antenna_type_name)));
    }
    if (attr_handle == ble_server_antenna_polarization_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_antenna_polarization_name, sizeof(ble_server_antenna_polarization_name)));
    }
    if (attr_handle == ble_server_antenna_freq_range_handle) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_antenna_freq_range_name, sizeof(ble_server_antenna_freq_range_name)));
    }
  }
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
    if (attr_handle == ble_server_antenna_type_handle && ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION)) == 0) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_antenna_type, sizeof(ble_server_antenna_type)));
    }
    if (attr_handle == ble_server_antenna_polarization_handle && ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION)) == 0) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_antenna_polarization, sizeof(ble_server_antenna_polarization)));
    }
    if (attr_handle == ble_server_antenna_freq_range_handle && ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION)) == 0) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &ble_server_antenna_freq_range, sizeof(ble_server_antenna_freq_range)));
    }
    // all properties utf8
    if (ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT)) == 0) {
      ERROR_CHECK_RESPONSE(os_mbuf_append(ctxt->om, &utf8_string_format, sizeof(utf8_string_format)));
    }

  }
  return 0;
}

static const struct ble_gatt_svc_def ble_antenna_items[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0xd5, 0x6a, 0x4b, 0x20, 0xa, 0xf1, 0x43, 0x85, 0x9e, 0xaf, 0x54, 0x63, 0x2c, 0x97, 0xd2, 0x3e),
        .characteristics = (struct ble_gatt_chr_def[])
            {{
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_MODEL_NAME_UUID),
                 .access_cb = ble_server_handle_antenna_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_antenna_model_name_handle
             },
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_SERVER_MANUFACTURER_NAME_UUID),
                 .access_cb = ble_server_handle_antenna_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_antenna_manuf_name_handle
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0xad, 0xd0, 0x35, 0xae, 0xb2, 0xff, 0x42, 0x7c, 0xad, 0x78, 0x59, 0x91, 0x8a, 0x76, 0xfb, 0x2f),
                 .access_cb = ble_server_handle_antenna_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_antenna_type_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_antenna_service,
                      },
                      {
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_antenna_service,
                      },
                      {
                          0
                      }}
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0x1a, 0x5e, 0xd0, 0x1d, 0xbe, 0x65, 0x46, 0x93, 0x9c, 0xc5, 0xf8, 0xd4, 0x8e, 0x95, 0xad, 0xbf),
                 .access_cb = ble_server_handle_antenna_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_antenna_polarization_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_antenna_service,
                      },
                      {
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_antenna_service,
                      },
                      {
                          0
                      }}
             },
             {
                 .uuid = BLE_UUID128_DECLARE(0xc0, 0x50, 0x1c, 0xe1, 0xf0, 0x22, 0x4d, 0x23, 0xb7, 0x3a, 0xbd, 0xbb, 0x35, 0x3f, 0xd8, 0x81),
                 .access_cb = ble_server_handle_antenna_service,
                 .flags = BLE_GATT_CHR_F_READ,
                 .val_handle = &ble_server_antenna_freq_range_handle,
                 .descriptors = (struct ble_gatt_dsc_def[])
                     {{
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_USER_DESCRIPTION),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_antenna_service,
                      },
                      {
                          .uuid = BLE_UUID16_DECLARE(BLE_SERVER_PRESENTATION_FORMAT),
                          .att_flags = BLE_ATT_F_READ,
                          .access_cb = ble_server_handle_antenna_service,
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

esp_err_t ble_antenna_svc_register() {
  ERROR_CHECK(ble_gatts_count_cfg(ble_antenna_items));
  ERROR_CHECK(ble_gatts_add_svcs(ble_antenna_items));
  return ESP_OK;
}