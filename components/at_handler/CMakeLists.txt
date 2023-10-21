
idf_component_register(SRCS "at_handler.c"
        INCLUDE_DIRS "." REQUIRES at_config display sx127x_util at_util ble_client at_timer)