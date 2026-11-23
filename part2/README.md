# Part 2 — MPI 1D Strip Decomposition

This folder contains the MPI strip-decomposed version of the 2D explicit heat solver. It builds on the serial reference in `../part1/`. The temperature grid is split into contiguous horizontal strips, and processes exchange ghost rows before each time step.

## Requirements

- Ubuntu/Linux, C++17, Open MPI (`mpic++`, `mpirun`)
- `python3` for the partition checker
- The built Part 1 serial executable at `../part1/heat` for the comparison script
- For distributed runs: reachable VMs, passwordless SSH, Tailscale and an MPI executable at the same path on each VM

## Build

From `part2/`:

```bash
make clean
make
```

The Makefile compiles `src/strips.cpp` to `./heat_strips` using `mpic++ -std=c++17 -O2 -ffp-contract=off`.

## Run locally

```bash
mpirun --oversubscribe -np 3 ./heat_strips \
  --decomp strips --case V --N 16 --r 0.2 --T 0.05 \
  --out results/mpi_V_N16_P3.bin
```

Options:

- `--decomp strips`: select this MPI implementation.
- `--case 1|2|V`: all four edges hot; top and bottom hot; or sine-mode verification.
- `--N`: number of intervals along each spatial direction; the full grid is `(N+1) × (N+1)`.
- `--r`: explicit heat-update parameter; requires `0 < r <= 0.25`.
- `--steps count` **or** `--T time`: choose the step count or desired final simulated time, not both. With `--T`, the program chooses a safe number of steps and may decrease the effective `r` to reach the requested time.
- `--out path`: optional full-grid binary output written by rank 0.
- `--debug-halos`: print each rank's local array, including ghost rows, after its first halo exchange. Do not enable during performance measurements.
- `--help`: display usage.

There must be no more MPI processes than interior rows: `P <= N-1`.

The summary contains one `key=value` line from rank 0: `decomp`, `P`, `case`, `N`, `r`, `steps`, `T`, `t_total`, `t_comp`, `t_comm`, and, for Case V, `err_inf` and `err_l2h`. The three timing fields are in seconds. Each is independently reduced across ranks using `MPI_MAX` and refers to the time-stepping phase, not initialization, gathering or file output. The maxima need not add exactly.

Output `.bin` files contain `(N+1)^2` consecutive `double` values, with `x` index varying fastest and no header. The tested x86-64 machines store doubles in little-endian order. Binaries are ignored by Git and can be regenerated.

## Tests and verification

Check strip distribution:

```bash
python3 tests/check_decomposition.py
```

Debug the required small-grid halo cases:

```bash
mpirun -np 2 ./heat_strips --decomp strips --case 2 --N 8 --r 0.2 --steps 1 --debug-halos
mpirun --oversubscribe -np 3 ./heat_strips --decomp strips --case 2 --N 8 --r 0.2 --steps 1 --debug-halos
```

Check numerical agreement with the serial solver:

```bash
bash tests/check_correctness.sh
```

The script compares binary output files with `cmp` for Cases 1, 2 and V, `N=8,10,16`, and `P=1,2,3,4`, at `r=0.2` for 10 steps: **36 comparisons**. The measured run passed **36/36**; see `results/step5_correctness_log.txt`. This is evidence for the tested configurations, not a proof of every possible input.

## Distributed runs over Tailscale

On the launch VM, create a hostfile listing accessible Tailscale IPs, for example:

```text
<heat-2-tailscale-IP> slots=1
<heat-1-tailscale-IP> slots=1
<heat-3-tailscale-IP> slots=1
```

Replace the placeholders with the VM addresses. For two VMs, use two lines and `-np 2`. Build `./heat_strips` on each VM in the same `~/part2` directory. Launch from `~/part2`:

```bash
mpirun --hostfile hosts_3vm -np 3 \
  --mca oob_tcp_if_include tailscale0 \
  --mca btl_tcp_if_include tailscale0 \
  --mca btl tcp,self \
  ./heat_strips --decomp strips --case 2 --N 8 --r 0.2 --steps 10 \
  --out results/distributed_case2_N8_P3.bin
```

Compare with an equivalent serial output using `cmp`. In our tests, Case 2 matched the serial output bit for bit on both two-VM and three-VM runs. An additional three-VM Case V run remains **unverified** because MPI lost connectivity to remote daemons; it should not be recorded as a successful test. Network connectivity and VM availability must be checked before attempting longer demonstrations.

## Implementation outline

- Share the `N-1` interior rows in balanced contiguous strips; lower ranks receive any extra rows.
- Store `(nlocal+2) × (N+1)` values per rank, with two ghost rows.
- Use two `MPI_Sendrecv` calls per step, with `MPI_PROC_NULL` for missing neighbors.
- Apply the same five-point stencil as the serial solver, then swap current/next buffers.
- Use `MPI_Gatherv` to reconstruct the complete field on rank 0.
- Compute Case V errors against the analytic sine-mode solution.

## Files

- `src/strips.cpp`: MPI implementation.
- `tests/check_decomposition.py`: checks balanced row assignment.
- `tests/check_correctness.sh`: repeatable serial/MPI binary comparisons.
- `results/step5_correctness_log.txt`: recorded 36/36 passing checks.
- `Makefile`: build instructions.
- `.gitignore`: excludes local compiled files and binary fields.

The team report describes the algorithm, proof of bit-for-bit agreement for tested cases, halo debugging, observed communication overhead, and connection limitations. A separate numerical-solutions document covers the related course exercises.
