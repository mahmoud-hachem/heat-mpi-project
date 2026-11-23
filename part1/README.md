# Part 1 — Serial heat solver (Step 2 working version)

This folder contains the active serial solver from the team project, with a minimal interface/output update. The numerical stencil, initialization, boundary cases, time-step selection, error calculations, and binary output are unchanged from the supplied `src/serial.cpp`.

## Build (Ubuntu / Linux)

```bash
make
```

This builds `./heat` using C++17, `-O2`, and `-ffp-contract=off`.

## Run

```bash
./heat --decomp serial --case V --N 16 --r 0.2 --T 0.05 --out verification.bin
./heat --decomp serial --case 1 --N 8 --r 0.2 --steps 10
./heat --decomp serial --case 2 --N 8 --r 0.2 --steps 1 --debug
```

Supported options: `--decomp serial`, `--case 1|2|V`, `--N`, `--r`, `--T` *or* `--steps`, `--out`, `--debug`, `--help`.

Only a **single `key=value` summary line** goes to standard output on successful runs. `--debug` prints the full grid to standard error. `t_total` is the step-loop duration in this serial program and equals `t_comp`; `t_comm=0`. Setup and binary output are excluded from the timing.

`--out` writes the `(N+1) x (N+1)` field as native binary doubles (little-endian on the course's x86-64 Ubuntu VMs), with `i` varying fastest.

## Step 2 change scope

- Accept the `--decomp serial` argument required by the common interface.
- Reject other decomposition modes in this **serial-only** executable.
- Build `./heat` in one command rather than `src/serial`.
- Print only one machine-readable summary on stdout, including `t_total`.
- Make full-grid printing opt-in via `--debug` (stderr).

The final project-wide `heat` integration with MPI strips and blocks is a later step. Convergence, instability scripts and retained results from the teammate's original archive will be brought into the final Part 1 directory in later steps.

## Verification performed when preparing this version

- `make` builds successfully with the flags above.
- Case V (`N=16`, `r=0.2`, `T=0.05`): field bytes identical to the original.
- Case 2 (`N=8`, `r=0.2`, three steps): field bytes identical to the original.
- Unsupported `--decomp blocks` and unstable `r=0.26` are rejected.

Keep the original archive as a backup. No claims are made here about a fresh three-VM MPI run; this file is the serial solver only.
