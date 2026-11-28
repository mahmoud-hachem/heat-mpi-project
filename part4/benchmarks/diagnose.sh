#!/usr/bin/env bash
# Diagnose why distributed runs stall: network path, VM health, or CPU
# starvation of tailscaled by busy-polling MPI ranks.
#
# Usage (on heat-2):  bash ~/part4/benchmarks/diagnose.sh HOSTFILE
#   e.g.              bash ~/part4/benchmarks/diagnose.sh ~/part4/hosts_amer
#
# Every step has a timeout, so the script cannot hang. Output is saved in
# ~/part4/results/diagnostics_<date>/ and printed as it runs.
#
# Experiments (all strips, Case 2, N=8192, r=0.24, 10 steps):
#   A  P=2, one rank per VM (map-by node)   -> each VM keeps one vCPU free
#   B  P=4, two ranks per VM (as in the benchmarks)
#   C  P=4, two ranks per VM, mpi_yield_when_idle=1 (waiting ranks yield)
# While each runs, top is sampled on both VMs: tailscaled CPU and steal (st).

set -uo pipefail

HOSTFILE="${1:?Usage: diagnose.sh HOSTFILE}"
OUT="$HOME/part4/results/diagnostics_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT"
LOG="$OUT/diagnose.log"
exec > >(tee -a "$LOG") 2>&1

mapfile -t HOSTS < <(awk '!/^[[:space:]]*(#|$)/ {print $1}' "$HOSTFILE")
LOCAL_ADDRS=" $(hostname) $(hostname -I 2>/dev/null) "
EXE="$HOME/part2/heat_strips"
MPI=(--hostfile "$HOSTFILE" --nooversubscribe
     --mca oob_tcp_if_include tailscale0 --mca btl_tcp_if_include tailscale0
     --mca btl tcp,self)

on() {  # run a command on a host, never longer than 10 s
    if [[ "$LOCAL_ADDRS" == *" $1 "* ]]; then timeout 10s bash -c "$2"
    else timeout 10s ssh -o BatchMode=yes -o ConnectTimeout=5 "$1" "$2"; fi
}

section() { echo; echo "===== $* ====="; }

section "1. VM health and leftovers"
for h in "${HOSTS[@]}"; do
    echo "--- $h"
    on "$h" 'hostname; uptime; nproc; free -h | head -2;
             echo "leftover solver processes:"; pgrep -a "heat_strips|heat_blocks|orted" || echo none;
             echo "tailscaled:"; ps -o pid,pcpu,rss,cmd -C tailscaled | tail -n +2' \
        || echo "UNREACHABLE: $h (SSH timed out)"
done

section "2. Tailscale path between the VMs (direct or DERP relay)"
for h in "${HOSTS[@]}"; do
    [[ "$LOCAL_ADDRS" == *" $h "* ]] && continue
    timeout 20s tailscale ping -c 5 "$h" || echo "tailscale ping to $h FAILED"
done
tailscale status 2>&1 | head -10

section "3. Raw TCP throughput over Tailscale (2 MB, no MPI)"
for h in "${HOSTS[@]}"; do
    [[ "$LOCAL_ADDRS" == *" $h "* ]] && continue
    S=$(date +%s.%N)
    if head -c 2000000 /dev/zero | timeout 60s ssh -o BatchMode=yes "$h" 'cat > /dev/null'; then
        E=$(date +%s.%N)
        awk -v s="$S" -v e="$E" -v h="$h" \
            'BEGIN { printf "%s: 2 MB in %.2f s (includes SSH start-up and encryption)\n", h, e - s }'
    else
        echo "$h: transfer FAILED or took over 60 s"
    fi
done

sample_top() {  # $1 = tag; samples every 2 s until killed
    while true; do
        for h in "${HOSTS[@]}"; do
            on "$h" 'echo "[$(hostname) $(date +%T)]"; top -b -n1 | sed -n "3p";
                     top -b -n1 -o %CPU | sed -n "8,11p"' >>"$OUT/top_$1.txt" 2>&1 \
                || echo "[$h $(date +%T)] top sample FAILED (VM not answering)" >>"$OUT/top_$1.txt"
        done
        sleep 2
    done
}

run_exp() {  # $1 tag, $2 P, $3 map, rest = extra mpirun args
    local tag=$1 P=$2 map=$3; shift 3
    section "Experiment $tag: P=$P map-by $map $*"
    for h in "${HOSTS[@]}"; do on "$h" 'pkill -u "$USER" -x heat_strips; true' >/dev/null 2>&1; done
    sample_top "$tag" & local SAMPLER=$!
    local S=$(date +%s)
    timeout --kill-after=10s 240s mpirun "${MPI[@]}" -np "$P" --map-by "$map" "$@" \
        "$EXE" --decomp strips --case 2 --N 8192 --r 0.24 --steps 10 \
        >"$OUT/exp_$tag.log" 2>&1
    local RC=$? E=$(date +%s)
    kill "$SAMPLER" 2>/dev/null; wait "$SAMPLER" 2>/dev/null
    echo "exit=$RC wall=$((E - S))s"
    grep "^decomp=" "$OUT/exp_$tag.log" || { echo "NO RESULT LINE"; tail -5 "$OUT/exp_$tag.log"; }
    echo "top samples (CPU line, busiest processes) -> $OUT/top_$tag.txt"
    grep -E "Cpu|tailscaled|heat_strips|FAILED" "$OUT/top_$tag.txt" | head -16
}

run_exp A 2 node
run_exp B 4 slot
run_exp C 4 slot --mca mpi_yield_when_idle 1

section "Done"
echo "Everything saved in $OUT"
echo "How to read it:"
echo " - Step 2 says 'via DERP'       -> traffic is relayed: network path problem."
echo " - A fast, B slow or stalled,"
echo "   tailscaled near 0% and st/us"
echo "   at 100% during B             -> CPU starvation: 2 busy ranks on 2 vCPUs leave"
echo "                                   no CPU for tailscaled to move the packets."
echo " - C much faster than B         -> confirms it; yielding ranks free the CPU."
echo " - top samples FAILED mid-run   -> the VM itself froze (VirtualBox/host issue)."
