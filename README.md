# Parallel 2D Heat Diffusion with MPI over Tailscale

USJ – ESIB course project. A 2D heat-equation solver (explicit FTCS
five-point stencil) parallelised with MPI and run on three laptops' VMs that
are linked by a Tailscale overlay network.

| Member | VM | Host laptop CPU |
| --- | --- | --- |
| Mahmoud Hachem | heat-2 (launch node) | Intel Core i7-10510U |
| Ali Harkous | heat-1 | AMD Ryzen 7 6800H |
| Amer Soulak | heat-3 | Intel Core i7-14700HX |

Each VM runs Ubuntu Server 24.04.5 with 2 vCPUs and 4 GiB of RAM, on VirtualBox
in bridged mode. All three use the shared account `mpi` with passwordless SSH
and Open MPI 4.1.6. MPI traffic is pinned to `tailscale0`.

## Repository layout

| Path | Content |
| --- | --- |
| [`part0/`](part0/README.md) | Cluster setup, MPI hello world, ping-pong latency and bandwidth, hardware and STREAM memory bandwidth |
| [`part1/`](part1/README.md) | Serial C++17 solver, Cases 1, 2 and V, convergence study, deliberate instability test |
| [`part2/`](part2/README.md) | MPI 1D strip decomposition (`MPI_Sendrecv`, `MPI_PROC_NULL`) and correctness tests |
| [`part3/`](part3/README.md) | MPI 2D block decomposition (`MPI_Cart_create`, `MPI_Type_vector`), strips vs blocks |
| [`part4/`](part4/README.md) | Strong and weak scaling, communication share, model vs measurement, crossover, slot vs node placement |
| [`docs/`](docs/) | Report (`heat-mpi-REPORT.docx`), 2-page summary (`.docx` and `.pdf`), 3 slides (`.pptx`) |
| [`llm_log/`](llm_log/README.md) | Full LLM conversations: Claude Code, Codex (with subagents), ChatGPT |
| [`AGENTS.md`](AGENTS.md) | How AI assistants were used, and the rules they followed |

## Quick start

Install on every VM:

```bash
sudo apt install -y build-essential openmpi-bin libopenmpi-dev python3-numpy python3-matplotlib
```

Clone the repository at the **same absolute path** on all three VMs, then
build each part with `make` (Open MPI does not copy binaries to remote hosts):

```bash
make -C part0 && make -C part1 && make -C part2 && make -C part3
```

Distributed runs use the Tailscale interface only:

```bash
mpirun --hostfile part3/hosts_3vm_P4 -np 4 \
  --mca oob_tcp_if_include tailscale0 --mca btl_tcp_if_include tailscale0 --mca btl tcp,self \
  "$(pwd)/part3/heat_blocks" --decomp blocks --case 2 --N 512 --r 0.2 --steps 100
```

Each part's README has the exact commands, test scripts and benchmark queues.

## Main results

- **Convergence.** In Case V the observed order of the L2 error is 2.0058,
  2.0014, 2.0004 and 2.0001: second order in h. The solver is stable for
  r ≤ 1/4. At r = 0.2625 a checkerboard error grows.
- **Correctness.** The parallel output matches the serial output bit for bit
  in 36/36 strip tests and 72/72 block tests. Distributed runs on two and
  three VMs also match.
- **Network.** Cross-VM links have t_s = 3.5–6.8 ms and 1.5–5.8 MB/s. On the
  same VM, t_s = 7–17 µs. Memory bandwidth is 10.7–22.9 GB/s, so the network
  dominates.
- **Strips vs blocks.** At P = 4 on heat-2 + heat-3, blocks are faster at
  N = 512, 2048 and 8192, as the placement-aware model predicts.
- **Crossover.** Distribution starts to pay off at about N = 8192: blocks on
  two VMs take 1.11 s against 1.30 s for P = 2 on one VM. For N ≤ 2048,
  staying on one VM is always faster.

Results from different networks (home Wi-Fi, café relay, phone hotspot) are
kept in separate folders and are never pooled. Failed and stalled attempts
are kept as evidence.

## Security

The repository holds no passwords, SSH private keys, Tailscale auth keys or
API tokens. The LLM logs were scanned and redacted before being committed
(see [`llm_log/README.md`](llm_log/README.md)).
