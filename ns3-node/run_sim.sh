#!/bin/env bash
# Copyright 2026 PES Innovation Lab
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

set -e

# Wait for nk0 to appear in the container namespace
echo "Waiting for network interface nk0..."
NK_FOUND=false
for _ in {1..15}; do
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
    # run a specific simulation and exit
    /app/ns-3/ns3 configure --build-profile="${BUILD_PROF:-optimized}" --enable-examples --enable-tests
    exec /app/ns-3/ns3 run "$SIM_NAME"
else
    # keep the container alive for exec-based usage
    exec tail -f /dev/null
fi
