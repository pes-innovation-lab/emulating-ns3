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

/app/ns-3/narrow-noarp-interfaces.sh

. /app/pyenv/bin/activate

SIM_NAME="$1"

# Copy scratch scripts from mounted scripts directory to root of scratch to preserve CMakeLists.txt
if [ -d "/app/ns-3/scratch/scripts" ]; then
    cp -r /app/ns-3/scratch/scripts/* /app/ns-3/scratch/ 2>/dev/null || true
    # Remove the scripts subdirectory reference inside the copied destination to avoid recursive copies
    rm -rf /app/ns-3/scratch/scripts/scripts 2>/dev/null || true
fi

if [ -n "$SIM_NAME" ]; then
    # run a specific simulation and exit
    /app/ns-3/ns3 configure --build-profile="${BUILD_PROF:-optimized}" --enable-examples --enable-tests
    exec /app/ns-3/ns3 run "$SIM_NAME"
else
    # keep the container alive for exec-based usage
    exec tail -f /dev/null
fi
