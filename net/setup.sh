#!/bin/sh
sudo ip link add br-left type bridge
sudo ip link add br-right type bridge
sudo ip tuntap add tap-left mode tap
sudo ip tuntap add tap-right mode tap
sudo ifconfig tap-left 0.0.0.0 promisc up
sudo ifconfig tap-right 0.0.0.0 promisc up
sudo ip link set tap-left master br-left
sudo ifconfig br-left up
sudo ip link set tap-right master br-right
sudo ifconfig br-right up
pushd /proc/sys/net/bridge
for f in bridge-nf-*; do echo 0 > $f; done
popd
