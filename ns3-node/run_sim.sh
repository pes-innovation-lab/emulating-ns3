#!/bin/env bash
set -e

. /app/pyenv/bin/activate

SIM_NAME="$1"

if [ -n "$SIM_NAME" ]; then
    # run a specific simulation and exit
    /app/ns-3/ns3 configure --build-profile="${BUILD_PROF:-optimized}" --enable-examples --enable-tests
    exec /app/ns-3/ns3 run "$SIM_NAME"
else
    # keep the container alive for exec-based usage
    exec tail -f /dev/null
fi
