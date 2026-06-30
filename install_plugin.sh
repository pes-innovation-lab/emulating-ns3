#!/usr/bin/env bash

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
sudo enable pair-ns-3
