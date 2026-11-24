#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")/.."

SERIAL="../part1/heat"
BLOCKS="./heat_blocks"

mkdir -p results

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

PASSED=0
FAILED=0
TOTAL=0

echo "=========================================="
echo "Part 3 - Automated Correctness Tests"
echo "=========================================="

for CASE in 1 2 V; do
    for N in 8 9 10 16; do

        SERIAL_OUT="$TMP_DIR/serial.bin"

        "$SERIAL" \
            --decomp serial \
            --case "$CASE" \
            --N "$N" \
            --r 0.2 \
            --steps 10 \
            --out "$SERIAL_OUT" > /dev/null

        for P in 1 2 3 4 6 8; do

            TOTAL=$((TOTAL + 1))

            MPI_OUT="$TMP_DIR/blocks.bin"

            rm -f "$MPI_OUT"

            if timeout 30s mpirun --oversubscribe \
                -np "$P" "$BLOCKS" \
                --decomp blocks \
                --case "$CASE" \
                --N "$N" \
                --r 0.2 \
                --steps 10 \
                --out "$MPI_OUT" > /dev/null \
                && cmp -s "$SERIAL_OUT" "$MPI_OUT"; then

                echo "PASS: Case=$CASE N=$N P=$P"

                PASSED=$((PASSED + 1))

            else

                echo "FAIL: Case=$CASE N=$N P=$P"

                FAILED=$((FAILED + 1))

            fi

        done
    done
done

echo "=========================================="
echo "TOTAL:  $TOTAL"
echo "PASSED: $PASSED"
echo "FAILED: $FAILED"
echo "=========================================="

if [ "$FAILED" -eq 0 ]; then
    echo "SUCCESS: All $TOTAL tests passed"
else
    echo "ERROR: Some tests failed"
    exit 1
fi
