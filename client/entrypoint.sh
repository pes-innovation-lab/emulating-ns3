#!/bin/bash
# Start FRR daemons
/usr/lib/frr/frrinit.sh start

# Execute python sniffer
exec python3 /app/main.py
