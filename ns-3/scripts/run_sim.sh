#!/bin/env bash

ip link set eth0 promisc on
ip link set eth0 up

sleep 5

. /app/pyenv/bin/activate
/app/ns-3/ns3 run $1
