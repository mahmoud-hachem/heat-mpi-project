# Part 0: Three-VM MPI cluster and network measurements

This folder preserves the team's completed Part 0 code, the six sets of
ping-pong benchmark measurements, initial latency/bandwidth estimates, and
plots. It is one component of the full heat-diffusion course project.

## Cluster

- Ubuntu Server 24.04 LTS virtual machines: `heat-1`, `heat-2`, `heat-3`.
- A common Linux username: `mpi`.
- VirtualBox and Tailscale overlay networking; passwordless OpenSSH.
- Two MPI process slots per VM (see `hosts`); Open MPI 4.1.6 was used.
- Same-VM tests and cross-VM tests were carried out using MPI over TCP.
- After moving VirtualBox from NAT to Bridged Adapter, Tailscale direct
  connectivity was observed on the shared Wi-Fi; early NAT tests were relayed.

**To document before final submission:** the physical CPU model, VM RAM,
network type and `tailscale ping` direct/DERP status for **every pair**, as
well as any requested screenshots/command output. Hardware and exact network
conditions are not proven by this archive alone.

## Prerequisites

```bash
sudo apt update
sudo apt install -y build-essential cmake git openmpi-bin libopenmpi-dev python3-numpy python3-matplotlib openssh-client
```

## Build

On **each** of the three Ubuntu VMs, clone/copy the repository under the
**same absolute path** and then run:

```bash
cd ~/heat-project/part0  # example path; keep the same path on all VMs
make
```

This creates `bin/hello` and `bin/pingpong` locally on each VM. Open MPI
launches the executables remotely; it does not automatically distribute them.

## Run MPI Hello World

From one VM, while in `part0`:

```bash
mpirun --hostfile hosts -np 6 \
  --mca oob_tcp_if_include tailscale0 \
  --mca btl_tcp_if_include tailscale0 \
  --mca btl tcp,self \
  "$(pwd)/bin/hello"
```

Expect two processes on each VM, six in total. The order of printed lines may
vary because processes run concurrently.

## Run a ping-pong measurement

Use exactly two processes: one on each host for cross-VM measurements, or
both on the same host for local measurements. For example:

```bash
bash scripts/run_benchmark.sh 'heat-1:1,heat-2:1' heat2_heat1_repeat.csv 10
bash scripts/run_benchmark.sh 'heat-2:2' same_heat2_repeat.csv 10
```

The helper refuses to overwrite an existing result. The benchmark sends
messages of 8, 16, ..., 16,777,216 bytes (16 MiB); each size gets 2 warm-up
round trips and 10 measured round trips (unless changed). It writes one-way
communication time as half the average round-trip time.

**Measurement detail:** `--mca btl tcp,self` forces MPI TCP communication even
for same-VM tests; local results therefore characterize MPI TCP on a single
VM, not optimized shared-memory MPI transport. On other Wi-Fi networks,
Tailscale may switch between direct and DERP relay paths.

## Analyze results

From any working directory, run:

```bash
python3 /absolute/path/to/part0/scripts/analyze_network.py
```

Or from `part0`: `python3 scripts/analyze_network.py`.

The script reads the six complete CSVs in `results/raw/`, and writes:

- `results/network_summary.csv` (latency in milliseconds, estimated effective
  bandwidth in decimal MB/s)
- `results/plots/same_vm_model.png`
- `results/plots/cross_vm_model.png`

For each pair, it estimates startup latency `t_s` as the median one-way time
for message sizes <= 1,024 bytes, and estimates transfer time per byte `t_w`
using a fixed-intercept least-squares fit for messages >= 1 MiB. The model is
`t_msg(m) = t_s + m*t_w`; bandwidth is `1/t_w`. These are **model estimates**
from one batch of measurements, not guaranteed physical link bandwidths.
Cross-VM measurements have significant variability, so repeated runs and a
spread estimate are advisable before using the data for definitive comparisons.

The two files in `results/raw/prebridge_incomplete/` are interrupted early
network measurements. They are retained as historical evidence but **are not
used** to fit the reported model and must not be mistaken for completed runs.

## Submission status

This folder includes the technical Part 0 source and recorded measurements.
The complete project submission still requires a root README for all parts,
Part 1/2/3 solvers, full experiment scripts/results, the separate mathematical
solutions PDF, correctness evidence, the report, and **full LLM prompts and
responses** in `llm_log/` together with `AGENTS.md` or `CLAUDE.md` at the
repository root. A summary is **not** a substitute for those logs.
