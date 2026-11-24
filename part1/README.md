# Part 1 — Serial heat solver

This solver advances the 2D heat equation on the unit square using the explicit five-point stencil and two time-level arrays. It is the serial reference for the MPI strips and blocks solvers.

## Build (Ubuntu / Linux)

```bash
make
```

Run these commands from `part1/`. The default build produces `./heat` with C++17, `-O2`, and `-ffp-contract=off`. The plotting scripts require Python 3 and Matplotlib.

## Run

```bash
./heat --decomp serial --case V --N 16 --r 0.2 --T 0.05 --out verification.bin
./heat --decomp serial --case 1 --N 8 --r 0.2 --steps 10
./heat --decomp serial --case 2 --N 8 --r 0.2 --steps 1 --debug
```

| Option | Meaning |
| --- | --- |
| `--decomp serial` | Select the serial solver. This is the default. |
| `--case 1\|2\|V` | Four hot edges, hot horizontal edges with cold side interiors, or the exact-solution verification case. Case 2 corners are hot. |
| `--N` | Number of intervals per axis. Grid spacing is `h = 1/N`. |
| `--r` | Requested upper bound on `dt/h²`, with `0 < r <= 0.25`. |
| `--T` | Final simulated time. The solver chooses enough equal steps to reach it without exceeding the requested `r`. |
| `--steps` | Fixed step count at the requested `r`. Use either this or `--T`. |
| `--out` | Write the final grid to a raw binary file. |
| `--debug` | Print the final grid to standard error. |
| `--help` | Print usage. |

Successful runs print one `key=value` summary line. `t_total` and `t_comp` both measure the step loop in seconds, and `t_comm=0`. Initialization, error evaluation, and file output are outside the timing interval. Case V also reports `err_inf` and `err_l2h` against the exact solution.

`--out` writes the full `(N+1) × (N+1)` field, including boundaries, in row-major order with `i` varying fastest. Values are native binary doubles, which are little-endian on the project's x86-64 Ubuntu VMs. The file has no header.

## Convergence study

Task 7.2 uses Case V, `r=0.2`, and `T=0.05` on five grids:

```bash
for N in 16 32 64 128 256; do
    ./heat --decomp serial --case V --N "$N" --r 0.2 --T 0.05
done
python3 scripts/plot_convergence.py
```

The plotting script reads the saved [solver output](results/convergence_runs.txt) and regenerates the [convergence table](results/convergence.csv) and [log-log plot](results/convergence.png). The measured discrete L2 error drops by about a factor of four on each refinement. The observed order approaches 2.

## Deliberate instability test

Task 6.7 uses the separate experimental executable at `r=0.2625`, 5% above the production solver's stability bound:

```bash
make unstable_demo
./unstable_demo --case V --N 16 --r 0.2625 --steps 1000 --out results/instability_1000.bin
python3 scripts/plot_instability.py
```

The saved [1,000-step output](results/instability_1000.txt) and [checkerboard image](results/instability.png) show growing grid-scale oscillations. Earlier logs in `results/` record how the error develops.
