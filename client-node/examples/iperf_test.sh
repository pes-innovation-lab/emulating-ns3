#!/bin/sh
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

NS3_NODE_IP="10.10.0.1"
PORT=5001
PROTOCOL="tcp"
DURATION=10
UDP_BANDWIDTH="10M"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --protocol)
      PROTOCOL="$2"
      shift 2
      ;;
    --duration)
      DURATION="$2"
      shift 2
      ;;
    --bandwidth)
      UDP_BANDWIDTH="$2"
      shift 2
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--protocol tcp|udp] [--duration seconds] [--bandwidth rate (e.g. 10M)]"
      exit 1
      ;;
  esac
done

# Ensure protocol is lowercase
PROTOCOL=$(echo "$PROTOCOL" | tr '[:upper:]' '[:lower:]')

# Check connectivity first
if ! ping -c 1 "${NS3_NODE_IP}" >/dev/null 2>&1; then
  echo "Error: Cannot reach ns-3 node at ${NS3_NODE_IP}."
  echo "Make sure the ns-3 container is running the simulation."
  exit 1
fi

if [ "$PROTOCOL" = "udp" ]; then
  echo "Running UDP iperf test to ${NS3_NODE_IP}:${PORT} with bandwidth ${UDP_BANDWIDTH} for ${DURATION} seconds..."
  iperf -c "${NS3_NODE_IP}" -p "${PORT}" -u -b "${UDP_BANDWIDTH}" -t "${DURATION}"
else
  echo "Running TCP iperf test to ${NS3_NODE_IP}:${PORT} for ${DURATION} seconds..."
  iperf -c "${NS3_NODE_IP}" -p "${PORT}" -t "${DURATION}"
fi
