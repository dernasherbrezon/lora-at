cmake_minimum_required(VERSION 3.16)
project(test_host)

enable_testing()

add_test(NAME test_at_handler COMMAND test_at_handler)
add_executable(test_at_handler
        "at_handler_test.c"
        "../components/at_util/at_util.c"
        "../components/at_handler/at_handler.c"
        "./mock/Mockat_config.c"
        "./mock/Mockdisplay.c"
        "./mock/Mocksx127x_util.c"
        "./mock/Mockble_client.c"
        "./mock/Mocksx127x.c"
        "./mock/esp_err.c"
        "unity-2.5.2/src/unity.c"
        "cmock-2.5.3/src/cmock.c"
)
target_include_directories(test_at_handler PUBLIC
        "../components/at_handler/"
        "../components/at_config/"
        "../components/at_util/"
        "../components/ble_client/"
        "../components/display/"
        "../components/sx127x_util/"
        "../managed_components/dernasherbrezon__sx127x/include"
        "unity-2.5.2/src/"
        "cmock-2.5.3/src/"
        "./mock"
)

add_test(NAME test_at_util COMMAND test_at_util)
add_executable(test_at_util
        "at_util_test.c"
        "unity-2.5.2/src/unity.c"
        "cmock-2.5.3/src/cmock.c"
        "../components/at_util/at_util.c"
)
target_include_directories(test_at_util PUBLIC
        "../components/at_util/"
        "./unity-2.5.2/src/"
        "./mock"
)
