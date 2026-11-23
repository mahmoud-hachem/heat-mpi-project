Part 2 Step 5: CLI and Timing Output Patch

Based on verified halo-debug strips.cpp; no changes to row decomposition,
MPI_Sendrecv, stencil, swapping, or MPI_Gatherv.

Adds --decomp strips, --help, --steps/--T mutual exclusion, stable r check;
prints one key=value line to stdout (except --debug-halos).
Errors reported as err_inf/err_l2h only for Case V. All times in seconds.
--out optional, matches serial solver. One P=1..4 re-test required.

INSTALL (Windows PowerShell):
  Expand-Archive -LiteralPath "$env:USERPROFILE\Downloads\Part2_Step5_Interface_Patch.zip" -DestinationPath "C:\Users\User\Desktop\heat-mpi-project\part2" -Force
  scp "C:\Users\User\Desktop\heat-mpi-project\part2\src\strips.cpp" mpi@192.168.44.117:/home/mpi/part2/src/

UBUNTU heat-2:
  cd ~/part2 && make clean && make
  mpirun --oversubscribe -np 3 ./heat_strips --decomp strips --case V --N 16 --r 0.2 --T 0.05 --out results/step5_mpi_V_N16_P3.bin
  ../part1/heat --decomp serial --case V --N 16 --r 0.2 --T 0.05 --out results/step5_serial_V_N16.bin
  cmp results/step5_serial_V_N16.bin results/step5_mpi_V_N16_P3.bin && echo 'PASS: identical'

Timing caveat: debug-halos prints and barriers inside timed step loop. Only normal runs are valid for timing comparisons.
