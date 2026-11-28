#!/usr/bin/env bash
# Hardware and memory-bandwidth survey of every VM in a hostfile.
# Run once, from the launch VM (heat-2):
#   bash ~/part0/scripts/hardware_info.sh [HOSTFILE=~/part4/hosts_all]
# For each host it records CPU (lscpu), vCPUs, memory, GPU, and runs a
# STREAM-style memory bandwidth test (part0/src/stream.cpp) with 1 and 2 threads.
# Output: ~/part0/results/hardware/<host>.txt and summary.csv (never overwritten:
# a new run goes into a timestamped folder if results already exist).
set -uo pipefail

HOSTFILE=${1:-$HOME/part4/hosts_all}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC="$ROOT/src/stream.cpp"
OUT="$ROOT/results/hardware"
[ -e "$OUT/summary.csv" ] && OUT="$ROOT/results/hardware_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT"
[ -f "$SRC" ] || { echo "missing $SRC" >&2; exit 1; }

HOSTS=$(awk '!/^[[:space:]]*#/ && NF {print $1}' "$HOSTFILE")
LOCAL=" $(hostname) $(hostname -I 2>/dev/null) localhost 127.0.0.1 "

# Commands run on each host (sent over stdin); stream.cpp is copied to ~/hwbench first.
REMOTE='
set -u
cd ~/hwbench
echo "=== host: $(hostname)  date: $(date "+%F %T %Z")"
echo "=== lscpu"; lscpu
echo "=== nproc: $(nproc)"
echo "=== free -m"; free -m
echo "=== GPU"
if command -v lspci >/dev/null; then lspci | grep -i -E "vga|3d|display" || echo "no GPU device listed"; else echo "lspci not installed"; fi
if command -v nvidia-smi >/dev/null; then nvidia-smi -L; else echo "nvidia-smi: not present"; fi
echo "=== compiler: $(g++ --version | head -1)"
if g++ -O3 -march=native -fopenmp -o stream stream.cpp 2>/dev/null; then echo "built with OpenMP"
else g++ -O3 -march=native -o stream stream.cpp && echo "built WITHOUT OpenMP (2-thread run = 1 thread)"; fi
for t in 1 2; do echo "=== stream OMP_NUM_THREADS=$t"; OMP_NUM_THREADS=$t ./stream 20000000 10; done
'

echo "host,cpu_model,vcpus,mem_total_MB,gpu,copy_1t_GBs,triad_1t_GBs,copy_2t_GBs,triad_2t_GBs" > "$OUT/summary.csv"
for h in $HOSTS; do
    echo "--- $h"
    if [[ "$LOCAL" == *" $h "* ]]; then
        mkdir -p ~/hwbench && cp "$SRC" ~/hwbench/
        bash -c "$REMOTE" > "$OUT/tmp.txt" 2>&1
    else
        ssh -o BatchMode=yes -o ConnectTimeout=10 "$h" 'mkdir -p ~/hwbench' &&
        scp -q -o BatchMode=yes "$SRC" "$h:hwbench/" &&
        timeout 300 ssh -o BatchMode=yes "$h" bash -s <<< "$REMOTE" > "$OUT/tmp.txt" 2>&1 ||
        { echo "FAILED on $h (see $OUT/tmp.txt)"; mv "$OUT/tmp.txt" "$OUT/failed_$h.txt"; continue; }
    fi
    name=$(awk '/^=== host:/ {print $3; exit}' "$OUT/tmp.txt")
    mv "$OUT/tmp.txt" "$OUT/${name:-$h}.txt"
    f="$OUT/${name:-$h}.txt"
    model=$(awk -F: '/^Model name/ {gsub(/^[ \t]+/,"",$2); print $2; exit}' "$f" | tr ',' ';')
    vcpus=$(awk '/^=== nproc:/ {print $3}' "$f")
    mem=$(awk '/^Mem:/ {print $2; exit}' "$f")
    gpu=$(awk '/^=== GPU/ {getline; print; exit}' "$f" | tr ',' ';')
    bw() { awk -v t="$1" -v k="$2" '/^=== stream OMP_NUM_THREADS=/ {cur=substr($3,17)} cur==t && $1==k {split($2,a,"="); print a[2]}' "$f"; }
    echo "${name:-$h},$model,$vcpus,$mem,$gpu,$(bw 1 copy),$(bw 1 triad),$(bw 2 copy),$(bw 2 triad)" >> "$OUT/summary.csv"
    tail -1 "$OUT/summary.csv"
done
echo "Wrote $OUT"
