#/bin/bash

echo "this is helper interactive script to configure connection between ESP32 and bluetooth server on the targer host"
echo "if ESP32 board already configured for deep sleep mode, then simply curl web proxy with pre-configured values"

SERIAL_PORT=""
echo "enter serial device:"
read SERIAL_PORT
if [[ ${SERIAL_PORT} == "" ]]; then
    echo "serial port must be non-empty"
    exit 1
fi

INACTIVITY_TIMEOUT_DEFAULT=30000
echo "enter 'inactivity timeout' (default=${INACTIVITY_TIMEOUT_DEFAULT}):"
read INACTIVITY_TIMEOUT
if [[ ${INACTIVITY_TIMEOUT} == "" ]]; then
    INACTIVITY_TIMEOUT = ${INACTIVITY_TIMEOUT_DEFAULT}
fi

DEEP_SLEEP_PERIOD_DEFAULT=30000
echo "enter 'deep sleep period' (default=${DEEP_SLEEP_PERIOD_DEFAULT}):"
read DEEP_SLEEP_PERIOD
if [[ ${DEEP_SLEEP_PERIOD} == "" ]]; then
    DEEP_SLEEP_PERIOD = ${DEEP_SLEEP_PERIOD_DEFAULT}
fi

SERVER_BT_ADDRESS="$(hcitool dev | awk '$0=$2')"

echo "Server BT address is: ${SERVER_BT_ADDRESS}"
echo "Ready for ESP configuration"
echo "Proceed? (y/n)"
read PROCEED
if [[ ${PROCEED} != "y" ]]; then
    exit 0
fi

COMMAND="AT+DSCONFIG=${SERVER_BT_ADDRESS},${INACTIVITY_TIMEOUT},${DEEP_SLEEP_PERIOD}"

echo -e ${COMMAND} > ${SERIAL_PORT}

DEVICE_INFO=""

while true
do
    REPLY=$(cat ${SERIAL_PORT})
    echo "reply: ${REPLY}"
    if [[ ${REPLY} == "OK" ]]; then
        break
    elif [[ ${REPLY} == "ERROR" ]]; then
        break
    elif [[ ${REPLY} =~ ([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}) ]]; then
        DEVICE_INFO=${REPLY}
    fi
done

if [[ ${DEVICE_INFO} == "" ]]; then
    exit 1
fi

DEVICE_INFO_ARR=(${DEVICE_INFO//,/ })

curl -X POST "http://localhost:8080/api/v1/client?client=${DEVICE_INFO_ARR[0]}&minFreq=${DEVICE_INFO_ARR[1]}&maxFreq=${DEVICE_INFO_ARR[2]}"

echo "client with address ${DEVICE_INFO_ARR[0]} configured. Min frequency: ${DEVICE_INFO_ARR[1]} Max frequency: ${DEVICE_INFO_ARR[2]}"