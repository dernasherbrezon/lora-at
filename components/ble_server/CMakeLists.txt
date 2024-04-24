set(srcs "")
if(CONFIG_BT_ENABLED)
    list(APPEND srcs "ble_server.c")
else()
    list(APPEND srcs "no_ble_server.c")
endif()

idf_component_register(SRCS ${srcs}
        INCLUDE_DIRS "."
        REQUIRES bt nvs_flash at_sensors)