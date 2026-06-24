#!/bin/sh
# Connectivity test: ping the ns-3 simulator node (10.10.0.1).

NS3_NODE_IP="10.10.0.1"

echo "Pinging ns-3 node at ${NS3_NODE_IP}..."
if ping -c 3 "${NS3_NODE_IP}" >/dev/null 2>&1; then
    echo "Successfully reached ${NS3_NODE_IP}"
    exit 0
else
    echo "Could not reach ${NS3_NODE_IP}"
    echo "   Check that the ns-3 container is running and the network plugin is installed."
    exit 1
fi
