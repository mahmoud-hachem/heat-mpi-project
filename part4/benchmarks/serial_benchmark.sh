#!/usr/bin/env bash
set -euo pipefail

cd "$HOME/part4"

STEPS=10
R=0.24
CASE=2
REPEATS=3
SOLVER="$HOME/part1/heat"

HOST=$(hostname)
DIR=$(mktemp -d "results/serial_${HOST}_XXXXXX")

for N in 512 1024 2048 4096 8192; do
    for REP in $(seq 1 "$REPEATS"); do

        LOG="$DIR/serial_N${N}_R${REP}.log"

        echo "Host=$HOST N=$N Run=$REP"

        timeout 120s "$SOLVER" \
            --decomp serial \
            --case "$CASE" \
            --N "$N" \
            --r "$R" \
            --steps "$STEPS" \
            2>&1 | tee "$LOG"

    done
done

echo "SERIAL BENCHMARK COMPLETED"
echo "Results saved in: $DIR"
