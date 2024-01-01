set(srcs "")
if(CONFIG_BT_ENABLED)
    list(APPEND srcs "ble_client.c")
else()
    list(APPEND srcs "no_ble_client.c")
endif()

idf_component_register(SRCS ${srcs}
        INCLUDE_DIRS "."
        REQUIRES bt nvs_flash sx127x_util)