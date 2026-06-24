#!/bin/sh
set -e

# Uncomment to disable transmit checksum offload
# ethtool -K nk0 tx off >/dev/null 2>&1 || true

if [ "$#" -eq 0 ]; then
    exec /bin/sh
fi

exec "$@"
