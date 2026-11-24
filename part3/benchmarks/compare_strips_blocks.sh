#!/usr/bin/env bash

set -euo pipefail

cd "$HOME/part3"

STEPS=10
R=0.24
CASE=2

MODE="${1:-pilot}"

if [ "$MODE" = "pilot" ]; then
    GRID_SIZES=(512)
    PROCESS_COUNTS=(4)
    REPEATS=1
elif [ "$MODE" = "full" ]; then
    GRID_SIZES=(512 2048 8192)
    PROCESS_COUNTS=(4 6)
    REPEATS=3
else
    echo "Usage: $0 pilot|full"
    exit 1
fi

DIR=$(mktemp -d "results/benchmark_${MODE}_XXXXXX")

echo "Results directory: $DIR"

for P in "${PROCESS_COUNTS[@]}"; do
    for N in "${GRID_SIZES[@]}"; do
        for REP in $(seq 1 "$REPEATS"); do
            for METHOD in strips blocks; do

                if [ "$METHOD" = "strips" ]; then
                    EXE="$HOME/part2/heat_strips"
                else
                    EXE="$HOME/part3/heat_blocks"
                fi

                LOG="$DIR/${METHOD}_N${N}_P${P}_R${REP}.log"

                echo ""
                echo "=================================="
                echo "Method=$METHOD N=$N P=$P Run=$REP"
                echo "=================================="

                if timeout --kill-after=10s 120s \
                    mpirun \
                    --hostfile "hosts_benchmark_P${P}" \
                    -np "$P" \
                    --nooversubscribe \
                    --map-by slot \
                    --mca oob_tcp_if_include tailscale0 \
                    --mca btl_tcp_if_include tailscale0 \
                    --mca btl tcp,self \
                    "$EXE" \
                    --decomp "$METHOD" \
                    --case "$CASE" \
                    --N "$N" \
                    --r "$R" \
                    --steps "$STEPS" \
                    2>&1 | tee "$LOG"; then

                    if grep -q "^decomp=$METHOD " "$LOG"; then
                        echo "SUCCESS"
                    else
                        echo "FAILED: Missing solver summary"
                        exit 1
                    fi
                else
                    RC=$?
                    echo "FAILED: Exit code $RC" | tee -a "$LOG"
                    echo "Stopping benchmark."
                    exit "$RC"
                fi

            done
        done
    done
done

echo ""
echo "BENCHMARK COMPLETED"
echo "Results saved in: $DIR"
