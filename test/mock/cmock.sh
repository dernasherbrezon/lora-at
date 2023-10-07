#!/bin/sh

BASEDIR=$(pwd)

ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -oat_config_mock_config.yaml ../../components/at_config/at_config.h

ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -odisplay_mock_config.yaml ../../components/display/display.h

ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -osx127x_util_mock_config.yaml ../../components/sx127x_util/sx127x_util.h

ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -osx127x_mock_config.yaml ../../managed_components/dernasherbrezon__sx127x/include/sx127x.h

ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -oble_client_mock_config.yaml ../../components/ble_client/ble_client.h

