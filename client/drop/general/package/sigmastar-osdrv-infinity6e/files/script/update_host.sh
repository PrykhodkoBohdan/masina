#!/bin/bash

# Configuration
CONFIG_TXT_FILE="/root/config.txt"

# Read the host from /root/config.txt
ENDPOINT_HOST=$(grep -E '^host=' "$CONFIG_TXT_FILE" | cut -d'=' -f2)

if [ -z "$ENDPOINT_HOST" ]; then
    echo "Error: No host found in $CONFIG_TXT_FILE."
    exit 1
fi

# Run yaml-cli command
yaml-cli -s .outgoing.server udp://"$ENDPOINT_HOST":2222
if [ $? -eq 0 ]; then
    echo "yaml-cli command executed successfully."
else
    echo "Error: Failed to execute yaml-cli command."
    exit 1
fi
