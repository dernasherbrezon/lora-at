#/bin/bash

# FIXME rewrite in python

#TODO 
# read inactivity timeout
INACTIVITY_TIMEOUT=30000

# TODO
# read deep sleep period
DEEP_SLEEP_PERIOD=30000

SERVER_BT_ADDRESS="FIXME"

echo "Ready for ESP configuration"
echo "Proceed? (y/n)"

# TODO 
# check if user said "y"

COMMAND="AT+DSCONFIG=${SERVER_BT_ADDRESS},${INACTIVITY_TIMEOUT},${DEEP_SLEEP_PERIOD}"

# TODO send the command via screen terminal

CLIENT_BT_ADDRESS="FIXME"

# TODO update configuration.json with the client address