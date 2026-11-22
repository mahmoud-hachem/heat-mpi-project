#!/usr/bin/env bash
# Example: bash scripts/run_benchmark.sh 'heat-1:1,heat-2:1' heat2_heat1_new.csv 10
set -euo pipefail
if (( $# < 2 || $# > 3 )); then
  echo "Usage: $0 HOST_SPEC OUTPUT_CSV [REPETITIONS=10]" >&2
  exit 2
fi
HOST_SPEC=$1
OUT_NAME=$2
REPS=${3:-10}
if [[ ! "$OUT_NAME" =~ ^[A-Za-z0-9_-]+\.csv$ ]]; then
  echo 'OUTPUT_CSV must be a simple CSV filename' >&2
  exit 2
fi
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUTPUT="$PROJECT_ROOT/results/raw/$OUT_NAME"
if [[ -e "$OUTPUT" ]]; then
  echo "Refusing to overwrite existing measurements: $OUTPUT" >&2
  exit 1
fi
# Build the same executable path on all participating VMs before running this.
mpirun --host "$HOST_SPEC" -np 2 \
  --mca oob_tcp_if_include tailscale0 \
  --mca btl_tcp_if_include tailscale0 \
  --mca btl tcp,self \
  "$PROJECT_ROOT/bin/pingpong" "$REPS" | tee "$OUTPUT"
