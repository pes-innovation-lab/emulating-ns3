#!/bin/sh
echo 'Waiting for containers to be ready...'
while [ -z "$PID1" ] || [ -z "$PID2" ]; do
    PID1=$(curl --unix-socket /var/run/docker.sock http://localhost/containers/"$C1"/json | jq -r '.State.Pid // empty')
    PID2=$(curl --unix-socket /var/run/docker.sock http://localhost/containers/"$C2"/json | jq -r '.State.Pid // empty')
    sleep 1
done

echo 'Linking network namespaces...'
mkdir -p /var/run/netns
ln -sf /proc/"$PID1"/ns/net /var/run/netns/ns1
ln -sf /proc/"$PID2"/ns/net /var/run/netns/ns2

echo 'Creating netkit link in L2 mode...'
ip link del nk1 2>/dev/null || true
ip link add nk1 type netkit mode l2 peer name nk2

echo 'Moving devices into containers...'
ip link set nk1 netns ns1
ip link set nk2 netns ns2

echo 'Configuring IPs, MACs and bringing interfaces up...'
ip netns exec ns1 ip link set nk1 address "$MAC1"
ip netns exec ns1 ip link set nk1 promisc on
ip netns exec ns1 ip link set nk1 up

ip netns exec ns2 ip link set nk2 address "$MAC2"
ip netns exec ns2 ip addr add "$IP2"/24 dev nk2
ip netns exec ns2 ethtool -K nk2 tx off
ip netns exec ns2 ip link set nk2 up

echo 'Netkit connection successfully established!'
