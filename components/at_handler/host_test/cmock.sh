#!/bin/sh

BASEDIR=$(pwd)
cd components/driver/
ruby ${IDF_PATH}/components/cmock/CMock/lib/cmock.rb -o../../mock_config.yaml --subdir=driver include/driver/uart.h
cd ${BASEDIR}