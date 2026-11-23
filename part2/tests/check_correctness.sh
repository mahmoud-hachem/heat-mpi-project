#!/usr/bin/env bash
set -euo pipefail

mkdir -p results/correctness

passed=0

for N in 8 10 16; do
  for CASE in 1 2 V; do

    SERIAL="results/correctness/serial_${CASE}_${N}.bin"

    ../part1/heat --decomp serial \
      --case "$CASE" --N "$N" \
      --r 0.2 --steps 10 \
      --out "$SERIAL" > /dev/null

    for P in 1 2 3 4; do

      MPI="results/correctness/mpi_${CASE}_${N}_${P}.bin"

      mpirun --oversubscribe -np "$P" ./heat_strips \
        --case "$CASE" --N "$N" \
        --r 0.2 --steps 10 \
        --out "$MPI" > /dev/null

      if cmp -s "$SERIAL" "$MPI"; then
        echo "PASS: Case=$CASE N=$N P=$P"
        passed=$((passed + 1))
      else
        echo "FAIL: Case=$CASE N=$N P=$P"
        exit 1
      fi

    done
  done
done

echo "--------------------------------"
echo "TOTAL: $passed/36 tests passed"
