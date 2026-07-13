#!/bin/bash
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
#
# Narrows every NOARP network interface (e.g. a netkit-l3 pair) from its
# docker/IPAM-assigned subnet-width address down to a host-scoped /32 (or
# /128 for IPv6), preventing re-broadcasting of ns-3's replies for NOARP interfaces.
set -uo pipefail

# Wait for at least one non-loopback interface to appear in the container
for _ in $(seq 1 15); do
    if ip -o link show | grep -qv '^[0-9]*: lo:'; then
        break
    fi
    sleep 0.5
done

narrow_v4() {
    local name="$1"
    ip -4 -o addr show dev "$name" scope global | awk '{print $4}' | while read -r addr; do
        [ -n "$addr" ] || continue
        local ip_only="${addr%%/*}"
        local prefix="${addr##*/}"
        if [ "$prefix" != "32" ]; then
            echo "narrow-noarp-interfaces: narrowing ${name} ${addr} -> ${ip_only}/32"
            ip addr del "$addr" dev "$name"
            ip addr add "${ip_only}/32" dev "$name"
        fi
    done
}

narrow_v6() {
    local name="$1"
    ip -6 -o addr show dev "$name" scope global | awk '{print $4}' | while read -r addr; do
        [ -n "$addr" ] || continue
        local ip_only="${addr%%/*}"
        local prefix="${addr##*/}"
        if [ "$prefix" != "128" ]; then
            echo "narrow-noarp-interfaces: narrowing ${name} ${addr} -> ${ip_only}/128"
            ip -6 addr del "$addr" dev "$name"
            ip -6 addr add "${ip_only}/128" dev "$name"
        fi
    done
}

for name in $(ip -o link show | awk -F': ' '{print $2}' | cut -d'@' -f1); do
    [ "$name" != "lo" ] || continue
    flags=$(ip -o link show "$name" 2>/dev/null | grep -o '<[^>]*>' | head -1)
    case "$flags" in
    *NOARP*)
        narrow_v4 "$name"
        narrow_v6 "$name"
        ;;
    esac
done
