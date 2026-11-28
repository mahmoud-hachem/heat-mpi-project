# Runbook: finishing the Part 3 and Part 4 measurements

## Why the earlier N=8192 runs "froze"

`heat_strips` always gathered the full grid to rank 0 with `MPI_Gatherv`, even
when no `--out` file was given, and printed its summary line only after that.
At N=8192, P=4, the two ranks on heat-1 had to send about 268 MB over Tailscale
(measured about 1.75 MB/s), which takes about 150 s, longer than the 120 s timeout. The time
steps finished, but the run was killed during the gather, before it printed
anything. That is why every `strips_N8192_*` log is empty. The old script
then stopped at the first failure, so blocks N=8192 was never attempted.

The fix: `heat_strips` now gathers only when `--out` is given or for Case V,
which is what `heat_blocks` already did. The time-stepping loop and the timed
interval are unchanged, so earlier strips timings stay valid.

## 1. Copy the updated files to heat-2 (Windows PowerShell, in the repo)

Check heat-2's address first (`hostname -I` on heat-2). Tailscale also works:
100.98.24.109.

```powershell
scp part2/src/strips.cpp mpi@100.98.24.109:~/part2/src/
scp -r part4/benchmarks part4/analysis part4/hosts_p8_example mpi@100.98.24.109:~/part4/
scp part3/benchmarks/finish_part3.sh mpi@100.98.24.109:~/part3/benchmarks/
```

## 2. Rebuild on all three VMs and re-check correctness (heat-2)

Every VM runs its own `~/part2/heat_strips`, so all three must be rebuilt.

```bash
cd ~/part2 && make
for h in 100.100.125.10 100.113.165.26; do
  scp ~/part2/src/strips.cpp mpi@$h:~/part2/src/ && ssh mpi@$h 'cd ~/part2 && make'
done
bash tests/check_correctness.sh          # expect: TOTAL: 36/36 tests passed
```

A distributed spot check (heat-2 + heat-1), with and without the gather:

```bash
cd ~/part2
../part1/heat --decomp serial --case 2 --N 10 --r 0.2 --steps 10 --out /tmp/s10.bin
timeout 60s mpirun --hostfile ~/part4/hosts_ali -np 4 --nooversubscribe --map-by slot \
  --mca oob_tcp_if_include tailscale0 --mca btl_tcp_if_include tailscale0 --mca btl tcp,self \
  ./heat_strips --decomp strips --case 2 --N 10 --r 0.2 --steps 10 --out /tmp/m10.bin
cmp /tmp/s10.bin /tmp/m10.bin && echo PASS
timeout 120s mpirun --hostfile ~/part4/hosts_ali -np 4 --nooversubscribe --map-by slot \
  --mca oob_tcp_if_include tailscale0 --mca btl_tcp_if_include tailscale0 --mca btl tcp,self \
  ./heat_strips --decomp strips --case 2 --N 8192 --r 0.24 --steps 10
```

The last command should now print its `decomp=strips ...` line within seconds.

## 3. Run the campaigns (heat-2)

Run them under `nohup` so a dropped SSH session does not kill them. Each
command can be re-run at any time: finished repetitions are skipped, failed
attempts are kept as `*.failed.log`.

Priority order:

```bash
# Part 3: strips vs blocks, P=4, heat-2 + heat-1, N = 512, 2048, 8192
cd ~/part3
nohup bash benchmarks/finish_part3.sh > campaign_P4.out 2>&1 &
tail -f campaign_P4.out

# Part 4: strong scaling, slot placement (missing configurations only)
cd ~/part4
nohup bash benchmarks/run_campaign.sh benchmarks/queues/part4_strong_slot.txt \
  hosts_all results/campaign_strong_slot > strong_slot.out 2>&1 &

# Part 4: weak scaling
nohup bash benchmarks/run_campaign.sh benchmarks/queues/part4_weak_slot.txt \
  hosts_all results/campaign_weak_slot > weak_slot.out 2>&1 &

# Part 4: node placement (lowest priority, 90 runs)
nohup bash benchmarks/run_campaign.sh benchmarks/queues/part4_strong_node.txt \
  hosts_all results/campaign_strong_node > strong_node.out 2>&1 &
```

Run one campaign at a time. Wait for `Campaign pass finished` before starting
the next, otherwise two campaigns would share the vCPUs.

To preview without running: prefix the command with `DRY_RUN=1`.

If the script stops with `STOP: cluster check failed`, fix the network
(`tailscale status`, `ssh` to each VM), then re-run the same command.

### P=8 (optional)

Only after the VMs really have 8 vCPUs in total (VirtualBox, VM powered off,
Settings, System, Processor). Edit `~/part4/hosts_p8_example` to match, then:

```bash
cd ~/part3 && nohup bash benchmarks/finish_part3.sh p8 ~/part4/hosts_p8_example > campaign_P8.out 2>&1 &
```

## 4. Bring the results back and analyse (Windows)

On heat-2:

```bash
cd ~ && tar czf campaign_results.tar.gz part3/results/campaign_* part4/results/campaign_*
```

In Windows PowerShell, in the repo:

```powershell
scp mpi@100.98.24.109:~/campaign_results.tar.gz .
tar xzf campaign_results.tar.gz
python part4/analysis/consolidate.py
```

`consolidate.py` rewrites `part4/results/consolidated/*.csv` and
`part4/plots/*.png` from every log, including the new campaigns.
