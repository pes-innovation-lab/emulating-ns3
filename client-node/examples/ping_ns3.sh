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
