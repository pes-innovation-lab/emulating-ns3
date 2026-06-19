#!/bin/env bash

cd ./plugin || (echo "missing plugin dir" && exit)

# steps taken from https://docs.docker.com/engine/extend/
docker build -t rootfsimage .
id=$(docker create rootfsimage true)
if [ -d "../plugin-data/rootfs" ]; then
    sudo rm -rf ../plugin-data/rootfs
fi
mkdir -p ../plugin-data/rootfs
sudo docker export "$id" | sudo tar -x -C ../plugin-data/rootfs
docker rm -vf "$id"
docker rmi rootfsimage

cd ..
sudo docker plugin create netkit-ns-3 ./plugin-data
sudo docker plugin enable netkit-ns-3
