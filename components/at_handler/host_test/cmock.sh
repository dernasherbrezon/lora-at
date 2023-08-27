#!/bin/sh

BASEDIR=$(pwd)
cd components/driver/
ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -o../../mock_config.yaml --subdir=driver include/driver/uart.h
cd ${BASEDIR}

cd components/at_config/
ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -o../../mock_config.yaml ../../../../at_config/at_config.h
cd ${BASEDIR}

cd components/display/
ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -o../../mock_config.yaml ../../../../display/display.h
cd ${BASEDIR}