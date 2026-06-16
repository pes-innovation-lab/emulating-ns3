#!/bin/env bash
. /app/pyenv/bin/activate
/app/ns-3/ns3 configure --build-profile="${BUILD_PROF:-optimized}" --enable-examples --enable-tests
/app/ns-3/ns3 run $1
