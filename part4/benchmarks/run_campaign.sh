#!/usr/bin/env bash
# Resumable benchmark runner for Part 3 and Part 4.
#
# Usage:
#   bash benchmarks/run_campaign.sh QUEUE_FILE HOSTFILE OUTDIR [REPEATS]
#
# QUEUE_FILE lines:  METHOD N P MAP     (blank lines and # comments ignored)
#   METHOD = strips | blocks,  MAP = slot | node
#
# Behaviour:
#   - Repetition-major order: run 1 of every configuration, then run 2, ...
#     so strips and blocks alternate and network drift is spread out.
#   - A repetition whose run_R.log already holds a solver summary is skipped,
#     so the same command can simply be re-run after any interruption.
#   - Failed attempts are kept as attempt_*.failed.log, never deleted.
#   - After a failure, leftover heat_strips/heat_blocks/orted processes owned
#     by this user are killed on every host in the hostfile.
#   - After STOP_AFTER consecutive failures, the cluster is re-checked; if the
#     check fails the script stops (exit 2) and can be resumed later.
#
# Environment overrides (defaults match all earlier benchmarks):
#   STEPS=10 R=0.24 CASE=2 BTL=tcp,self IFACE=tailscale0
#   MAX_ATTEMPTS=2 STOP_AFTER=4 TIMEOUT=<seconds, default depends on N>
#   DRY_RUN=1  -> print what would run, run nothing.

set -uo pipefail

QUEUE="${1:?Usage: run_campaign.sh QUEUE HOSTFILE OUTDIR [REPEATS]}"
HOSTFILE="${2:?Missing hostfile}"
OUTDIR="${3:?Missing output directory}"
REPEATS="${4:-3}"

STEPS="${STEPS:-10}"
R="${R:-0.24}"
CASE="${CASE:-2}"
BTL="${BTL:-tcp,self}"
IFACE="${IFACE:-tailscale0}"
MAX_ATTEMPTS="${MAX_ATTEMPTS:-2}"
STOP_AFTER="${STOP_AFTER:-4}"
DRY_RUN="${DRY_RUN:-0}"

STRIPS_EXE="${STRIPS_EXE:-$HOME/part2/heat_strips}"
BLOCKS_EXE="${BLOCKS_EXE:-$HOME/part3/heat_blocks}"

[ -f "$QUEUE" ] || { echo "Queue file not found: $QUEUE"; exit 1; }
[ -f "$HOSTFILE" ] || { echo "Hostfile not found: $HOSTFILE"; exit 1; }

mkdir -p "$OUTDIR"
CAMPAIGN_LOG="$OUTDIR/campaign.log"

say() { echo "[$(date '+%F %T')] $*" | tee -a "$CAMPAIGN_LOG"; }

# Hosts and total slots from the hostfile.
mapfile -t HOSTS < <(awk '!/^[[:space:]]*(#|$)/ {print $1}' "$HOSTFILE")
TOTAL_SLOTS=$(awk '!/^[[:space:]]*(#|$)/ {
    s = 1
    for (i = 2; i <= NF; i++) if ($i ~ /^slots=/) { split($i, a, "="); s = a[2] }
    t += s
} END { print t + 0 }' "$HOSTFILE")

MPI_ARGS=(
    --hostfile "$HOSTFILE"
    --nooversubscribe
    --mca oob_tcp_if_include "$IFACE"
    --mca btl_tcp_if_include "$IFACE"
    --mca btl "$BTL"
)

timeout_for() {
    local N=$1
    if [ -n "${TIMEOUT:-}" ]; then echo "$TIMEOUT"
    elif [ "$N" -le 2048 ]; then echo 120
    elif [ "$N" -le 4096 ]; then echo 240
    else echo 420
    fi
}

LOCAL_ADDRS=" $(hostname) $(hostname -I 2>/dev/null) localhost 127.0.0.1 "

run_on_host() {
    # Run a short command on a host: directly if it is this VM (mpirun does
    # not need SSH to itself, and the VM may not trust its own key),
    # otherwise via passwordless SSH.
    if [[ "$LOCAL_ADDRS" == *" $1 "* ]]; then
        timeout 15s bash -c "$2"
    else
        timeout 15s ssh -o BatchMode=yes -o ConnectTimeout=5 "$1" "$2"
    fi
}

cleanup_hosts() {
    local h
    for h in "${HOSTS[@]}"; do
        run_on_host "$h" \
            'pkill -u "$USER" -x heat_strips; pkill -u "$USER" -x heat_blocks; pkill -u "$USER" -x orted; true' \
            >/dev/null 2>&1 || say "WARN: cleanup could not reach $h"
    done
    sleep 3
}

preflight() {
    local h
    say "Preflight: checking SSH to ${HOSTS[*]}"
    for h in "${HOSTS[@]}"; do
        if ! run_on_host "$h" true >/dev/null 2>&1; then
            say "PREFLIGHT FAIL: SSH to $h"
            return 1
        fi
    done
    cleanup_hosts
    say "Preflight: mpirun hostname on $TOTAL_SLOTS slots"
    if ! timeout --kill-after=5s 30s mpirun "${MPI_ARGS[@]}" \
            -np "$TOTAL_SLOTS" --map-by slot hostname \
            >>"$CAMPAIGN_LOG" 2>&1; then
        say "PREFLIGHT FAIL: mpirun hostname"
        return 1
    fi
    say "Preflight OK"
}

write_meta() {
    local DIR=$1 METHOD=$2 N=$3 P=$4 MAP=$5 EXE=$6
    [ -f "$DIR/meta.txt" ] && return
    {
        echo "method=$METHOD"
        echo "N=$N"
        echo "P=$P"
        echo "map=$MAP"
        echo "case=$CASE"
        echo "r=$R"
        echo "steps=$STEPS"
        echo "btl=$BTL"
        echo "iface=$IFACE"
        echo "launch_host=$(hostname)"
        echo "created=$(date '+%F %T %Z')"
        echo "exe=$EXE"
        echo "exe_md5=$(md5sum "$EXE" 2>/dev/null | cut -d' ' -f1)"
        echo "hostfile=$HOSTFILE"
        echo "--- hostfile contents ---"
        cat "$HOSTFILE"
    } > "$DIR/meta.txt"
}

# Read the queue.
mapfile -t CONFIGS < <(awk '!/^[[:space:]]*(#|$)/ {print $1, $2, $3, $4}' "$QUEUE")
[ "${#CONFIGS[@]}" -gt 0 ] || { echo "Queue is empty"; exit 1; }

say "Campaign start: queue=$QUEUE hostfile=$HOSTFILE outdir=$OUTDIR repeats=$REPEATS btl=$BTL"

if [ "$DRY_RUN" != 1 ]; then
    preflight || { say "STOP: cluster not ready"; exit 2; }
fi

CONSECUTIVE=0

for REP in $(seq 1 "$REPEATS"); do
    for CFG in "${CONFIGS[@]}"; do
        read -r METHOD N P MAP <<<"$CFG"

        case "$METHOD" in
            strips) EXE="$STRIPS_EXE" ;;
            blocks) EXE="$BLOCKS_EXE" ;;
            *) say "SKIP invalid method in queue: $CFG"; continue ;;
        esac
        case "$MAP" in slot|node) ;; *) say "SKIP invalid map: $CFG"; continue ;; esac

        if [ "$P" -gt "$TOTAL_SLOTS" ]; then
            say "SKIP $CFG: P=$P exceeds $TOTAL_SLOTS slots in $HOSTFILE"
            continue
        fi

        DIR="$OUTDIR/${METHOD}_N${N}_P${P}_${MAP}"
        LOG="$DIR/run_${REP}.log"
        TO=$(timeout_for "$N")

        if grep -q "^decomp=$METHOD " "$LOG" 2>/dev/null; then
            echo "SKIP done: $LOG"
            continue
        fi

        if [ "$DRY_RUN" = 1 ]; then
            echo "WOULD RUN: rep=$REP $METHOD N=$N P=$P map=$MAP timeout=${TO}s -> $LOG"
            continue
        fi

        mkdir -p "$DIR"
        write_meta "$DIR" "$METHOD" "$N" "$P" "$MAP" "$EXE"

        for ATTEMPT_NO in $(seq 1 "$MAX_ATTEMPTS"); do
            ATTEMPT=$(mktemp "$DIR/attempt_XXXXXX")
            say "RUN rep=$REP $METHOD N=$N P=$P map=$MAP attempt=$ATTEMPT_NO timeout=${TO}s"
            START=$(date +%s)

            timeout --kill-after=10s "${TO}s" mpirun "${MPI_ARGS[@]}" \
                -np "$P" --map-by "$MAP" --display-map \
                "$EXE" --decomp "$METHOD" --case "$CASE" \
                --N "$N" --r "$R" --steps "$STEPS" \
                >"$ATTEMPT" 2>&1
            RC=$?
            ELAPSED=$(( $(date +%s) - START ))

            if [ "$RC" -eq 0 ] && grep -q "^decomp=$METHOD " "$ATTEMPT"; then
                echo "wall_seconds=$ELAPSED" >>"$ATTEMPT"
                mv "$ATTEMPT" "$LOG"
                say "OK   $(grep "^decomp=$METHOD " "$LOG")  (wall ${ELAPSED}s)"
                CONSECUTIVE=0
                break
            fi

            echo "FAILED: exit=$RC wall_seconds=$ELAPSED" >>"$ATTEMPT"
            mv "$ATTEMPT" "${ATTEMPT}.failed.log"
            say "FAIL exit=$RC after ${ELAPSED}s -> ${ATTEMPT}.failed.log"
            CONSECUTIVE=$((CONSECUTIVE + 1))
            cleanup_hosts

            if [ "$CONSECUTIVE" -ge "$STOP_AFTER" ]; then
                say "$CONSECUTIVE consecutive failures: re-checking cluster"
                if ! preflight; then
                    say "STOP: cluster check failed. Fix the network, then re-run the same command to resume."
                    exit 2
                fi
                CONSECUTIVE=0
            fi
        done
    done
done

# Progress report.
say "Campaign pass finished. Completed repetitions per configuration:"
for CFG in "${CONFIGS[@]}"; do
    read -r METHOD N P MAP <<<"$CFG"
    DIR="$OUTDIR/${METHOD}_N${N}_P${P}_${MAP}"
    DONE=$(grep -l "^decomp=$METHOD " "$DIR"/run_*.log 2>/dev/null | wc -l)
    FAILED=$(ls "$DIR"/*.failed.log 2>/dev/null | wc -l)
    say "  $METHOD N=$N P=$P $MAP : $DONE/$REPEATS done, $FAILED failed attempts"
done
