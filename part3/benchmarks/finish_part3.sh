#!/usr/bin/env bash
# Finish the Part 3 strips-vs-blocks measurements with the resumable runner.
#   bash benchmarks/finish_part3.sh          # P=4 on heat-2 + heat-1
#   bash benchmarks/finish_part3.sh amer     # P=4 on heat-2 + heat-3
#   bash benchmarks/finish_part3.sh p8 HOSTFILE   # P=8, needs 8 real vCPUs
# Re-run the same command to resume after an interruption.
set -uo pipefail
cd "$HOME/part3"
RUNNER="$HOME/part4/benchmarks/run_campaign.sh"
QUEUES="$HOME/part4/benchmarks/queues"

case "${1:-p4}" in
    p4) bash "$RUNNER" "$QUEUES/part3_P4.txt" "$HOME/part4/hosts_ali" \
            results/campaign_P4_ali 3 ;;
    amer) bash "$RUNNER" "$QUEUES/part3_P4.txt" "$HOME/part4/hosts_amer"             results/campaign_P4_amer 3 ;;
    p8) bash "$RUNNER" "$QUEUES/part3_P8.txt" "${2:?Give the 8-slot hostfile}" \
            results/campaign_P8 3 ;;
    *)  echo "Usage: $0 [p4 | amer | p8 HOSTFILE]"; exit 1 ;;
esac
