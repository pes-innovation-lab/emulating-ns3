#!/usr/bin/env bash
set -e

# Wait for nk0 to appear in the container namespace
echo "Waiting for network interface nk0..."
NK_FOUND=false
for i in {1..15}; do
    if ip link show dev nk0 >/dev/null 2>&1; then
        NK_FOUND=true
        break
    fi
    sleep 0.5
done

if [ "$NK_FOUND" = "true" ]; then
    echo "nk0 found. Flushing IP addresses from nk0..."
    ip addr flush dev nk0
else
    echo "nk0 not found. Available interfaces:"
    ip link show
    exit 1
fi

. /app/pyenv/bin/activate

SIM_NAME="$1"

if [ -n "$SIM_NAME" ]; then
    /app/ns-3/ns3 configure --build-profile="${BUILD_PROF:-optimized}" --enable-examples --enable-tests
    exec /app/ns-3/ns3 run "$SIM_NAME"
else
    # Keep the container alive for exec-based usage
    exec tail -f /dev/null
fi
