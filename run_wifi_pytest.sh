#!/usr/bin/env bash

cp ${IDF_PATH}/pytest.ini .
# make sure firmware with Wi-Fi support is built
idf.py fullclean
rm -f sdkconfig
SDKCONFIG_DEFAULTS="sdkconfig.wifi.0.ttgo-lora32-v2;~/sdkconfig.local" idf.py build
idf.py -p /dev/ttyACM0 flash

idf.py fullclean
rm -f sdkconfig
SDKCONFIG_DEFAULTS="sdkconfig.wifi.1.ttgo-lora32-v2;~/sdkconfig.local" idf.py build
idf.py -p /dev/ttyACM1 flash

# assume both devices are TTGO Lora V2
pytest --target esp32 --port="/dev/ttyACM0|/dev/ttyACM1" pytest_wifi.py