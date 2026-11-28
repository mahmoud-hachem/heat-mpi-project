
# Part 2 — MPI 1D Strip Decomposition

## Overview

This part implements the parallel version of the 2D explicit heat solver using MPI and 1D strip decomposition.

The temperature grid is divided into horizontal strips. Each MPI process computes its assigned rows and exchanges ghost rows with neighboring processes before each time step.

The serial solver from Part 1 is used as the reference for numerical correctness.

## Requirements

- Ubuntu/Linux
- C++17
- Open MPI
- Python 3
- Tailscale for distributed execution
- Passwordless SSH between VMs
- Part 1 serial executable at `../part1/heat`

## Project Structure

```text
part2/
├── src/
│   └── strips.cpp
├── tests/
│   ├── check_decomposition.py
│   └── check_correctness.sh
├── results/
│   ├── step5_correctness_log.txt
│   └── distributed_caseV_verification.txt
├── Makefile
├── README.md
└── .gitignore
```

## Implementation

The MPI solver implements:

- Balanced distribution of N-1 interior rows.
- Extra rows assigned to the lowest ranks.
- Two ghost rows per process.
- Halo exchange using MPI_Sendrecv.
- MPI_PROC_NULL for missing neighbors.
- Explicit five-point heat stencil.
- Two temperature arrays swapped after each step.
- Full grid reconstruction using MPI_Gatherv.
- Separate computation and communication timing.
- Maximum timing across ranks using MPI_Reduce and MPI_MAX.
- Case V verification against the exact mathematical solution.

## Compilation

From the part2 directory:

```bash
make clean
make
```

The Makefile uses:

```bash
mpic++ -std=c++17 -O2 -ffp-contract=off src/strips.cpp -o heat_strips
```

## Local Execution

Run Case V using three MPI processes on one machine:

```bash
mkdir -p results

mpirun --oversubscribe -np 3 ./heat_strips \
  --decomp strips \
  --case V \
  --N 16 \
  --r 0.2 \
  --T 0.05 \
  --out results/mpi_V_N16_P3.bin
```

## Command-Line Parameters

| Parameter | Description |
|---|---|
| --decomp strips | Select strip decomposition |
| --case 1 | All four boundaries hot |
| --case 2 | Top and bottom boundaries hot |
| --case V | Verification using an exact solution |
| --N | Number of intervals per spatial direction |
| --r | Numerical stability parameter |
| --steps | Number of time steps |
| --T | Requested final simulated time |
| --out | Binary output file |
| --debug-halos | Display local arrays and ghost rows |
| --help | Display available options |

The stability condition is:

```text
0 < r <= 0.25
```

The number of processes must satisfy:

```text
1 <= P <= N-1
```

Use either `--steps` or `--T`, not both. When using `--T`, the solver may reduce the effective time-step parameter to reach the requested final time while respecting stability.

## Output Format

The program prints one machine-readable summary line from rank 0.

Example:

```text
decomp=strips P=3 case=V N=16 r=0.2 steps=10 T=0.0078125 t_total=0.779550588 t_comp=8.19500000021e-06 t_comm=0.779543372 err_inf=0.000598805793956 err_l2h=0.000299402896978
```

- t_total: total time-stepping execution time.
- t_comp: computation time.
- t_comm: halo communication time.
- err_inf: maximum absolute error.
- err_l2h: discrete L2 error.

All execution times are measured in seconds.

Timing measurements exclude initialization, final gathering, binary output and error verification. Each timing field is independently reduced using MPI_MAX, so the maximum computation and communication times do not necessarily add up exactly to the maximum total time.

The error values are printed only for Case V.

Debug mode should not be used for performance measurements.

## Binary Output

The reconstructed temperature field is saved by rank 0.

The binary file contains:

- (N+1) x (N+1) temperature values.
- Double-precision floating-point values.
- Row-major ordering, with x varying fastest.
- No file header.
- Little-endian representation on the tested x86-64 VMs.

The format matches the Part 1 serial solver.

## Row Decomposition Tests

Run:

```bash
python3 tests/check_decomposition.py
```

The script checks N=8 and N=10 with P=1,2,3,4.

It verifies that every interior row belongs to exactly one process and that the row distribution is balanced.

All decomposition tests passed.

## Halo Exchange Debugging

The required small-grid tests use Case 2, N=8 and one time step.

### Two MPI processes

```bash
mpirun -np 2 ./heat_strips \
  --decomp strips --case 2 \
  --N 8 --r 0.2 --steps 1 \
  --debug-halos
```

### Three MPI processes

```bash
mpirun --oversubscribe -np 3 ./heat_strips \
  --decomp strips --case 2 \
  --N 8 --r 0.2 --steps 1 \
  --debug-halos
```

Both tests passed.

The three-process test verifies uneven decomposition, with three rows assigned to rank 0 and two rows assigned to each remaining rank.

Both resulting temperature fields were bit-for-bit identical to the serial reference.

## Automated Correctness Tests

Compile the Part 1 serial solver if necessary:

```bash
make -C ../part1
```

Run the correctness script from part2:

```bash
bash tests/check_correctness.sh
```

The script tests:

- Cases: 1, 2 and V.
- Grid sizes: N=8, N=10 and N=16.
- MPI processes: P=1, P=2, P=3 and P=4.
- Stability parameter: r=0.2.
- Time steps: 10.

Each MPI binary output is compared against the corresponding serial output using cmp.

Final result:

```text
TOTAL: 36/36 tests passed
```

All tested MPI configurations produced bit-for-bit identical results to the serial solver.

Evidence:

```text
results/step5_correctness_log.txt
```

## Distributed Execution over Tailscale

The project uses three Ubuntu VMs:

| VM | Tailscale IP |
|---|---|
| heat-1 | 100.100.125.10 |
| heat-2 | 100.98.24.109 |
| heat-3 | 100.113.165.26 |

All participating VMs must be powered on, connected to Tailscale and have the MPI executable available at the same path.

The solver is compiled on each VM in:

```text
/home/mpi/part2/
```

### Three-VM Hostfile

Create a file named `hosts_3vm`:

```text
100.98.24.109 slots=1
100.100.125.10 slots=1
100.113.165.26 slots=1
```

### Verify Connectivity

```bash
mpirun --hostfile hosts_3vm -np 3 \
  --mca oob_tcp_if_include tailscale0 \
  --mca btl_tcp_if_include tailscale0 \
  --mca btl tcp,self \
  hostname
```

The output must contain all three VM hostnames.

### Distributed Case 2

```bash
mpirun --hostfile hosts_3vm -np 3 \
  --mca oob_tcp_if_include tailscale0 \
  --mca btl_tcp_if_include tailscale0 \
  --mca btl tcp,self \
  ./heat_strips \
  --decomp strips --case 2 \
  --N 8 --r 0.2 --steps 10 \
  --out results/distributed_case2_N8_P3.bin
```

Generate the serial reference:

```bash
../part1/heat --decomp serial \
  --case 2 --N 8 --r 0.2 --steps 10 \
  --out results/serial_case2_N8_steps10.bin
```

Compare:

```bash
cmp results/serial_case2_N8_steps10.bin \
    results/distributed_case2_N8_P3.bin
```

Case 2 was successfully verified across both two and three VMs.

## Distributed Case V Verification

On November 24, 2026, Case V was successfully executed across all three Ubuntu VMs.

Parameters:

- N=16
- P=3
- r=0.2
- Steps=10
- T=0.0078125

### MPI Execution

```bash
timeout 60s mpirun --hostfile hosts_3vm -np 3 \
  --mca oob_tcp_if_include tailscale0 \
  --mca btl_tcp_if_include tailscale0 \
  --mca btl tcp,self \
  ./heat_strips \
  --decomp strips --case V \
  --N 16 --r 0.2 --steps 10 \
  --out results/distributed_caseV_short.bin
```

### Serial Reference

```bash
../part1/heat --decomp serial \
  --case V --N 16 --r 0.2 --steps 10 \
  --out results/serial_caseV_short.bin
```

### Correctness Verification

```bash
cmp results/serial_caseV_short.bin \
    results/distributed_caseV_short.bin
```

SHA-256 verification:

```bash
sha256sum results/serial_caseV_short.bin \
          results/distributed_caseV_short.bin
```

Both binary files had identical SHA-256 hashes.

Final result:

```text
PASS: Three-VM Case V, N=16, P=3, r=0.2, steps=10, bit-for-bit identical
```

### Case V Results

| Measurement | Serial | MPI - 3 VMs |
|---|---:|---:|
| t_total (s) | 0.000008669 | 0.779550588 |
| t_comp (s) | 0.000008669 | 0.000008195 | 
| t_comm (s) | 0 | 0.779543372 |
| err_inf | 0.000598805794 | 0.000598805794 |
| err_l2h | 0.000299402897 | 0.000299402897 |

Both implementations produced identical temperature fields and numerical errors.

Evidence:

```text
results/distributed_caseV_verification.txt
```

## Checkpoint C2 completed

On November 28, 2026, Case 2 passed distributed P=4 checks at N=8 and N=10 over heat-2 and heat-1. Both runs used r=0.2 and 10 steps. Independent byte-for-byte comparisons confirmed the recorded cmp results.

The [hostfile](hosts_c2_ali) assigns two slots to each VM. Slot placement puts ranks 0 and 1 on heat-2 (100.98.24.109) and ranks 2 and 3 on heat-1 (100.100.125.10). The [rank map](results/checkpoint_c2_ali/rank_placement.log) and both solver logs confirm that allocation. The team recorded TCP over Tailscale between VMs with vader shared-memory transport enabled within each VM.

| N | Interior rows per rank | Serial (s) | MPI total (s) | MPI compute (s) | MPI communication (s) | Comparison |
|---|---|---:|---:|---:|---:|---|
| 8 | 2, 2, 2, 1 | 0.000001766 | 1.230770150 | 0.000005149 | 1.230758227 | PASS |
| 10 | 3, 2, 2, 2 | 0.000002542 | 0.773958649 | 0.000004584 | 0.773950409 | PASS |

Both decompositions are uneven because N-1 is not divisible by four. The team recorded exit code 0 for both runs. The saved summaries and matching output fields confirm successful completion. These timings describe individual correctness runs.

### Saved evidence

- N=8 [serial summary](results/checkpoint_c2_ali/serial_N8.log), [MPI summary and map](results/checkpoint_c2_ali/strips_N8_P4.log), and [cmp result](results/checkpoint_c2_ali/compare_N8_P4.log).
- N=10 [serial summary](results/checkpoint_c2_ali/serial_N10.log), [MPI summary and map](results/checkpoint_c2_ali/strips_N10_P4.log), and [cmp result](results/checkpoint_c2_ali/compare_N10_P4.log).
- N=8 retained [serial field](results/checkpoint_c2_ali/serial_N8.bin) and [MPI field](results/checkpoint_c2_ali/strips_N8_P4.bin), each 648 bytes. SHA-256 is 6db3254208dd3322257f29ac684c2df596df50a9bc1869f867937e7132f2cba9.
- N=10 retained [serial field](results/checkpoint_c2_ali/serial_N10.bin) and [MPI field](results/checkpoint_c2_ali/strips_N10_P4.bin), each 968 bytes. SHA-256 is 6f5187f409defda578b0e41e5c76d1ed3ba31efd6fe768fedcd0da5fc0f21214.

### Reproduce the distributed P=4 comparisons

On Ubuntu heat-2, run from ~/part2 after compiling heat_strips at the same path on both VMs. Build the serial reference in ../part1. This creates a new output directory.

~~~bash
make -C ../part1
make
run_dir=$(mktemp -d "$PWD/results/c2_repeat_XXXXXX")
for n in 8 10; do
    serial="$run_dir/serial_N$n.bin"
    field="$run_dir/strips_N$n.bin"
    ../part1/heat --decomp serial --case 2 --N "$n" --r 0.2 --steps 10 \
        --out "$serial" > "$run_dir/serial_N$n.log"
    if timeout --kill-after=5s 60s mpirun --hostfile hosts_c2_ali -np 4 \
        --map-by slot --nooversubscribe --display-map \
        --mca oob_tcp_if_include tailscale0 \
        --mca btl_tcp_if_include tailscale0 --mca btl vader,tcp,self \
        ./heat_strips --decomp strips --case 2 --N "$n" --r 0.2 --steps 10 \
        --out "$field" > "$run_dir/strips_N$n.log" 2>&1
    then
        cmp "$serial" "$field" && echo "PASS: N=$n P=4 fields match" || break
    else
        echo "MPI run failed. Inspect $run_dir/strips_N$n.log"
        break
    fi
done
~~~

The 36 local comparisons, previous two- and three-VM checks, and these distributed P=4 checks complete the planned C2 technical verification. Professor checkpoint approval is recorded separately by the course.

### Earlier heat-3 attempts

The [heat-2 / heat-3 hostfile](hosts_c2) and [attempt evidence](results/checkpoint_c2/) are retained. The [rank map](results/checkpoint_c2/rank_placement.log) confirms process launching. The empty [MPI output](results/checkpoint_c2/strips_N8_P4.log) and [post-restart map](results/checkpoint_c2/rank_placement_after_restart.log) provide no completed solver result. The team reported frozen MPI execution and Linux kernel RCU CPU stall warnings on heat-3. Those warnings were not captured in these files. The cause remains unconfirmed. The later heat-1 runs completed successfully.


## Performance Observations

For small grids, MPI communication overhead is significantly larger than computation time.

Distributed execution over Tailscale can therefore be slower than serial execution, particularly when network latency is high.

The main objective of Part 2 is numerical correctness rather than speedup.

## Network Limitations

The original three-VM Case V experiment with 64 time steps was interrupted by intermittent network connectivity failures and MPI daemon disconnections.

A shorter experiment with 10 time steps was subsequently completed successfully across all three VMs.

The successful shorter experiment demonstrated bit-for-bit agreement with the serial solver. The original 64-step distributed experiment remains unverified.

Network connectivity should be checked before distributed demonstrations.

## Deadlock Prevention

The solver uses MPI_Sendrecv to exchange neighboring rows safely.

Separate blocking MPI_Send operations followed by MPI_Recv may cause deadlock when processes wait for matching receive operations.

MPI_Sendrecv combines sending and receiving in one operation and avoids this communication-ordering problem.

MPI_PROC_NULL is used for processes located at the physical boundaries of the plate.

## Final Verification Status

| Requirement | Status |
|---|---|
| Balanced strip decomposition | Passed |
| Ghost-row allocation | Passed |
| MPI_Sendrecv halo exchange | Passed |
| MPI_PROC_NULL handling | Passed |
| Halo debugging with P=2 | Passed |
| Halo debugging with P=3 | Passed |
| Cases 1, 2 and V | Passed locally |
| 36 automated correctness tests | 36/36 passed |
| Two-VM distributed Case 2 | Passed |
| Three-VM distributed Case 2 | Passed |
| Three-VM distributed Case V, 10 steps | Passed |
| Distributed P=4, N=8 and N=10 | Passed across heat-2 and heat-1 |
| Planned C2 technical verification | Completed |
| Bit-for-bit equality | Passed for tested configurations |
| SHA-256 verification | Passed |
| MPI timing measurements | Completed |

## Conclusion

Part 2 successfully implements MPI-based 1D strip decomposition.

The tested parallel results are bit-for-bit identical to the serial reference. Correctness was verified with different grid sizes, process counts, boundary conditions and distributed executions across multiple Ubuntu VMs.

This implementation provides the foundation for Part 3, which introduces 2D block decomposition.
