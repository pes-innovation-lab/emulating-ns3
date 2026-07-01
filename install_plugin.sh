#!/usr/bin/env bash
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


set -euo pipefail

cd ./plugin || {
    echo "missing plugin dir" >&2
    exit 1
}

# steps taken from https://docs.docker.com/engine/extend/
sudo docker build -t rootfsimage .
id=$(sudo docker create rootfsimage true)
if [ -d "../plugin-data/rootfs" ]; then
    sudo rm -rf ../plugin-data/rootfs
fi
mkdir -p ../plugin-data/rootfs
sudo docker export "$id" | sudo tar -x -C ../plugin-data/rootfs
sudo docker rm -vf "$id"
sudo docker rmi rootfsimage

cd ..
if sudo docker plugin inspect pair-ns-3 >/dev/null 2>&1; then
    sudo docker plugin disable -f pair-ns-3 || true
    sudo docker plugin rm -f pair-ns-3
fi
sudo docker plugin create pair-ns-3 ./plugin-data
sudo docker plugin enable pair-ns-3
