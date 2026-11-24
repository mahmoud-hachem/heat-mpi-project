#!/usr/bin/env bash
set -uo pipefail

cd "$HOME/part4"

ROOT=results/amer_batch
mkdir -p "$ROOT"

MPI_ARGS=(
    --hostfile hosts_amer
    --nooversubscribe
    --mca oob_tcp_if_include tailscale0
    --mca btl_tcp_if_include tailscale0
    --mca btl tcp,self
)

echo "Checking MPI connectivity..."

timeout --kill-after=5s 20s mpirun \
    "${MPI_ARGS[@]}" -np 4 --map-by slot hostname || exit 1

FAILURES=0

run_benchmark() {
    local METHOD=$1 N=$2 P=$3 MAP=$4
    local EXE DIR LOG ATTEMPT

    if [ "$METHOD" = strips ]; then
        EXE="$HOME/part2/heat_strips"
    else
        EXE="$HOME/part3/heat_blocks"
    fi

    DIR="$ROOT/${METHOD}_N${N}_P${P}_${MAP}"
    mkdir -p "$DIR"

    for REP in 1 2 3; do

        LOG="$DIR/run_${REP}.log"

        if grep -q "^decomp=$METHOD " "$LOG" 2>/dev/null; then
            echo "SKIP: $LOG already completed"
            continue
        fi

        ATTEMPT=$(mktemp "$DIR/attempt_XXXXXX")

        echo "RUN: $METHOD N=$N P=$P $MAP REP=$REP"

        if timeout --kill-after=5s 30s mpirun \
            "${MPI_ARGS[@]}" \
            -np "$P" --map-by "$MAP" \
            --display-map \
            "$EXE" \
            --decomp "$METHOD" \
            --case 2 --N "$N" --r 0.24 --steps 10 \
            2>&1 | tee "$ATTEMPT" \
            && grep -q "^decomp=$METHOD " "$ATTEMPT"
        then
            mv "$ATTEMPT" "$LOG"
            echo "SUCCESS"
            FAILURES=0
        else
            mv "$ATTEMPT" "${ATTEMPT}.failed.log"
            echo "FAILED: Moving to next configuration"
            FAILURES=$((FAILURES + 1))

            # Check for leftover solver processes
            if pgrep -u mpi -x 'heat_strips|heat_blocks' ||
               ! timeout 8s ssh -o BatchMode=yes \
                   -o ConnectTimeout=5 mpi@100.113.165.26 \
                   '! pgrep -u mpi -x "heat_strips|heat_blocks"'
            then
                echo "STOP: Check connection or leftover processes"
                exit 1
            fi

            if [ "$FAILURES" -ge 3 ]; then
                echo "STOP: Three consecutive failures"
                exit 1
            fi

            break
        fi
    done
}

# Priority 1: Weak scaling
for METHOD in strips blocks; do
    run_benchmark "$METHOD" 886 3 slot
    run_benchmark "$METHOD" 1023 4 slot
done

# Priority 2: Node placement
for P in 2 3 4; do
    for METHOD in strips blocks; do
        run_benchmark "$METHOD" 512 "$P" node
    done
done

# Priority 3: Large grid slot placement
for P in 3 4; do
    for METHOD in strips blocks; do
        run_benchmark "$METHOD" 8192 "$P" slot
    done
done

# Priority 4: Larger grid node placement
for N in 2048 8192; do
    for P in 2 3 4; do
        for METHOD in strips blocks; do
            run_benchmark "$METHOD" "$N" "$P" node
        done
    done
done

echo "BATCH FINISHED"
echo "Successful runs:"
find "$ROOT" -name 'run_*.log' | wc -l
