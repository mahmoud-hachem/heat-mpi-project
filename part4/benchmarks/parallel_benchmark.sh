#!/usr/bin/env bash
set -euo pipefail

cd "$HOME/part4"

METHOD="${1:?Specify strips or blocks}"
N="${2:?Specify grid size}"
P="${3:?Specify process count}"
MAP="${4:?Specify slot or node}"
HOSTFILE="${5:-hosts_all}"

STEPS=10
R=0.24
CASE=2
REPEATS=3

case "$METHOD" in
    strips) EXE="$HOME/part2/heat_strips" ;;
    blocks) EXE="$HOME/part3/heat_blocks" ;;
    *) echo "Invalid method"; exit 1 ;;
esac

case "$MAP" in
    slot|node) ;;
    *) echo "Invalid placement"; exit 1 ;;
esac

DIR=$(mktemp -d \
    "results/${METHOD}_N${N}_P${P}_${MAP}_XXXXXX")

for REP in $(seq 1 "$REPEATS"); do

    LOG="$DIR/run_${REP}.log"

    echo "Method=$METHOD N=$N P=$P Map=$MAP Run=$REP"

    if timeout --kill-after=10s 120s \
        mpirun \
        --hostfile "$HOSTFILE" \
        -np "$P" \
        --nooversubscribe \
        --map-by "$MAP" \
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

        grep -q "^decomp=$METHOD " "$LOG" || exit 1
        echo "SUCCESS"

    else
        echo "FAILED: Benchmark interrupted" | tee -a "$LOG"
        exit 1
    fi

done

echo "BENCHMARK COMPLETED"
echo "Results saved in: $DIR"
