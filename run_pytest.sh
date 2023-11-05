#!/usr/bin/env bash

cp ${IDF_PATH}/pytest.ini .
pytest --target esp32 --port="/dev/ttyACM0|/dev/ttyACM1" pytest_*