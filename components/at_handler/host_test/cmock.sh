#!/bin/sh

BASEDIR=$(pwd)

cd components/at_config/
ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -o../../mock_config.yaml ../../../../at_config/at_config.h
cd ${BASEDIR}

cd components/display/
ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -o../../mock_config.yaml ../../../../display/display.h
cd ${BASEDIR}

cd components/sx127x_util/
ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -o../../mock_config.yaml ../../../../sx127x_util/sx127x_util.h
cd ${BASEDIR}