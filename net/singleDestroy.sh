#!/bin/sh

# This file basically destroy the network bridge and TAP interface
# created by the singleSetup.sh script

if [ -z "$1" ]
  then
    echo "No name supplied"
    exit 1
fi

NAME=$1

sudo ifconfig br-$NAME down

sudo ip link set tap-$NAME nomaster

sudo ip link del br-$NAME

sudo ifconfig tap-$NAME down

sudo ip link del tap-$NAME