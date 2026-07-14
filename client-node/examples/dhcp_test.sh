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

# DHCP test: run perfdhcp against the ns-3 DHCP server (10.10.0.1).

NS3_SERVER_IP="10.10.0.1"
INTERFACE="nk0"

echo "Running perfdhcp client test on interface ${INTERFACE} against DHCP server at ${NS3_SERVER_IP}..."

# We run perfdhcp simulating 10 clients at a rate of 5 requests per second for a duration of 5 seconds.
perfdhcp -4 -l "${INTERFACE}" -r 5 -R 10 -p 5 -t 1 -xi "${NS3_SERVER_IP}"
