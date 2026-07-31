#!/bin/bash
# Batch runner: runs all protocols for N real-world runs each (the ns-3
# simulation side runs once when deterministic, N times otherwise).
set -e

RUNS="${1:-20}"
LABEL="${2:-batch}"
PROTOCOLS="tcp udp dhcp arp ping"

for proto in $PROTOCOLS; do
  echo "============================================================"
  echo "[$(date)] Starting $proto for $RUNS runs (label: $LABEL)"
  echo "============================================================"
  uv run -- evaluate.py --protocol "$proto" --runs "$RUNS" --timeout 120
  ret=$?
  if [ $ret -ne 0 ]; then
    echo "[$(date)] WARNING: $proto exited with code $ret, continuing"
  fi
  echo "[$(date)] Completed $proto"
  echo ""
done

echo "============================================================"
echo "[$(date)] All protocols finished for label: $LABEL"
echo "============================================================"
