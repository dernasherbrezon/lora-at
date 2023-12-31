set(wifi_srcs "")
if(CONFIG_AT_WIFI_ENABLED)
    list(APPEND wifi_srcs "at_wifi.c")
else()
    list(APPEND wifi_srcs "at_no_wifi.c")
endif()

idf_component_register(SRCS ${wifi_srcs}
        INCLUDE_DIRS "." REQUIRES nvs_flash esp_event esp_netif esp_wifi mdns)