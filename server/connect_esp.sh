#/bin/bash

SERIAL_PORT=$1

INACTIVITY_TIMEOUT_DEFAULT=30000
echo "inactivity timeout (default=${INACTIVITY_TIMEOUT_DEFAULT}):"
read INACTIVITY_TIMEOUT
if [[ ${INACTIVITY_TIMEOUT} eq "" ]]; then
    INACTIVITY_TIMEOUT = ${INACTIVITY_TIMEOUT_DEFAULT}
fi

DEEP_SLEEP_PERIOD_DEFAULT=30000
echo "deep sleep period (default=${DEEP_SLEEP_PERIOD_DEFAULT}):"
read DEEP_SLEEP_PERIOD
if [[ ${DEEP_SLEEP_PERIOD} eq "" ]]; then
    DEEP_SLEEP_PERIOD = ${DEEP_SLEEP_PERIOD_DEFAULT}
fi

# FIXME
SERVER_BT_ADDRESS="FIXME"

echo "Server BT address is: ${SERVER_BT_ADDRESS}"
echo "Ready for ESP configuration"
echo "Proceed? (y/n)"
read PROCEED
if [[ ${PROCEED} ne "y" ]]; then
    exit 0
fi

COMMAND="AT+DSCONFIG=${SERVER_BT_ADDRESS},${INACTIVITY_TIMEOUT},${DEEP_SLEEP_PERIOD}"

echo -e ${COMMAND} > ${SERIAL_PORT}

CLIENT_BT_ADDRESS=""

while true
do
    REPLY=$(cat ${SERIAL_PORT})
    echo "reply: ${REPLY}"
    if [[ ${REPLY} eq "OK" ]]; then
        break
    elif [[ ${REPLY} eq "ERROR" ]]; then
        break
    elif [[ ${REPLY} =~ ([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}) ]]; then
        CLIENT_BT_ADDRESS=${REPLY}
    fi
done

if [[ ${CLIENT_BT_ADDRESS} eq "" ]]; then
    exit 1
fi

#FIXME read min max freq via AT?
curl -X POST "http://localhost:8080/api/v1/client?client=${CLIENT_BT_ADDRESS}"

echo "client with address ${CLIENT_BT_ADDRESS} configured"