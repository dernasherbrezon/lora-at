set(srcs "")
if(CONFIG_AT_WIFI_ENABLED)
    list(APPEND srcs "at_rest.c")
else()
    list(APPEND srcs "at_no_rest.c")
endif()

idf_component_register(SRCS ${srcs}
        INCLUDE_DIRS "." REQUIRES sx127x_util esp_http_server json at_util)