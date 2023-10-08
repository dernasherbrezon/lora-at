idf_component_register(SRCS "ble_client.c"
        INCLUDE_DIRS "."
        REQUIRES bt nvs_flash sx127x_util)