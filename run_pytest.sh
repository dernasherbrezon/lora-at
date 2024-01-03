#!/usr/bin/env bash

cp ${IDF_PATH}/pytest.ini .
# make sure firmware for TTGO lora v2 is built
idf.py fullclean
SDKCONFIG_DEFAULTS="sdkconfig.ttgo-lora32-v2" idf.py build
# assume both devices are TTGO Lora V2
pytest --target esp32 --port="/dev/ttyACM0|/dev/ttyACM1" pytest_standalone.py