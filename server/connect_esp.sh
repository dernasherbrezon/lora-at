#/bin/bash

echo "this is helper script to configure connection between ESP32 and bluetooth server on the targer host"
echo "if ESP32 board already configured for deep sleep mode, then simply curl web proxy with pre-configured values"

SERIAL_PORT=$1
INACTIVITY_TIMEOUT=$2
DEEP_SLEEP_PERIOD=$3
SERVER_BT_ADDRESS="$(hcitool dev | awk '$0=$2')"
echo "Server BT address is: ${SERVER_BT_ADDRESS}"

COMMAND="AT+DSCONFIG=${SERVER_BT_ADDRESS},${INACTIVITY_TIMEOUT},${DEEP_SLEEP_PERIOD}"

echo -e ${COMMAND} > ${SERIAL_PORT}

DEVICE_INFO=""

while true
do
    read -d $'\n' -r REPLY < ${SERIAL_PORT}
    REPLY=${REPLY//[$'\t\r\n']}
    if [[ ${REPLY} == "" ]]; then
        sleep 1
        continue
    fi
    echo "reply: ${REPLY}"
    if [[ ${REPLY} == "OK" ]]; then
        break
    elif [[ ${REPLY} == "ERROR" ]]; then
        break
    elif [[ ${REPLY} =~ ([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2}) ]]; then
        DEVICE_INFO=${REPLY}
    fi
done

if [[ ${DEVICE_INFO} == "" ]]; then
    exit 1
fi

DEVICE_INFO_ARR=(${DEVICE_INFO//,/ })

curl -X POST "http://localhost:8080/api/v1/client?client=${DEVICE_INFO_ARR[0]}&minFreq=${DEVICE_INFO_ARR[1]}&maxFreq=${DEVICE_INFO_ARR[2]}"

echo "client with address ${DEVICE_INFO_ARR[0]} configured. Min frequency: ${DEVICE_INFO_ARR[1]} Max frequency: ${DEVICE_INFO_ARR[2]}"