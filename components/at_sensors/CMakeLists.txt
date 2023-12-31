set(srcs "")
if(CONFIG_SENSORS_ENABLED)
    list(APPEND srcs "at_sensors.c")
else()
    list(APPEND srcs "at_no_sensors.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS .
    REQUIRES ina219
)
