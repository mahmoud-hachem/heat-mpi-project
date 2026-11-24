\# Part 3 – MPI 2D Block Decomposition



\## Objective



Implement a parallel 2D heat diffusion solver using MPI with a two-dimensional block decomposition and compare its correctness and performance with the serial and 1D strip implementations.



\## Implementation



The solver uses:



\- MPI\_Dims\_create to determine the process grid dimensions.

\- MPI\_Cart\_create to create a non-periodic Cartesian topology.

\- MPI\_Cart\_shift to identify neighboring processes.

\- Balanced distribution of interior grid points.

\- Ghost cells on all four sides of each local block.

\- MPI\_Type\_vector for non-contiguous column communication.

\- MPI\_Sendrecv for halo exchanges.

\- MPI\_Gatherv for global grid reconstruction.

\- MPI\_Wtime for computation and communication measurements.



Corner ghost cells are not exchanged because the five-point stencil does not require diagonal neighbors.



\## Compilation



```bash

cd part3

make

```



The generated executable is `heat\_blocks`.



\## Execution



\### Local execution



```bash

mpirun -np 4 ./heat\_blocks --decomp blocks --case 2 --N 16 --r 0.2 --steps 10

```



\### Distributed execution over Tailscale



```bash

mpirun --hostfile hosts\_3vm\_P4 -np 4 \\

&#x20; --map-by slot \\

&#x20; --mca oob\_tcp\_if\_include tailscale0 \\

&#x20; --mca btl\_tcp\_if\_include tailscale0 \\

&#x20; --mca btl tcp,self \\

&#x20; ./heat\_blocks --decomp blocks --case 2 \\

&#x20; --N 16 --r 0.2 --steps 10 \\

&#x20; --out results/case2.bin

```



Update the hostfiles according to the available machines and their Tailscale addresses.



The executable must be compiled and available at the same path on every participating VM.



\## Simulation cases



\- Case 1: Four hot boundaries.

\- Case 2: Hot top and bottom boundaries and cold lateral boundaries, with hot corners.

\- Case V: Analytical verification using a sinusoidal initial temperature and zero boundary temperatures.



\## Correctness verification



The automated correctness tests cover:



\- Grid sizes N = 8, 9, 10 and 16.

\- Process counts P = 1, 2, 3, 4, 6 and 8.

\- All three simulation cases.



Result: 72/72 tests passed.



Distributed execution was also verified using multiple VMs. The results were identical to the serial reference in the tested configurations.



To run the automated tests:



```bash

bash tests/check\_correctness.sh

```



Compile the Part 1 reference solver before running the tests. Local tests with more processes than available CPU slots use oversubscription and are not performance benchmarks.



\## Simulation results



\### Case 1



!\[Case 1](results/demo\_blocks\_case1.png)



\### Case 2



!\[Case 2](results/demo\_blocks\_case2.png)



\### Case V



!\[Case V](results/demo\_blocks\_caseV.png)



These demonstrations use N = 16, P = 4, r = 0.2 and 10 time steps.



\## Performance comparison



The performance experiments compare the 1D strip and 2D block implementations.



Experimental configuration:



\- Two Ubuntu VMs connected through Tailscale.

\- Four MPI processes.

\- Case 2.

\- r = 0.24.

\- 10 time steps.

\- Three repetitions per method and grid size.



\### Median execution times



| Grid size | Processes | 1D Strips (s) | 2D Blocks (s) | Strips/Blocks ratio |

| --- | --- | --- | --- | --- |

| 512 | 4 | 3.3820 | 2.0395 | 1.66 |

| 2048 | 4 | 6.9643 | 4.4194 | 1.58 |

| 8192 | 4 | Not completed | Not completed | N/A |



The 2D block decomposition achieved lower median execution times for both completed grid sizes.



Communication dominated the execution time because the processes communicated across the Tailscale network.



The results support the theoretical prediction for the tested P = 4 configurations. However, network variability caused significant differences between individual repetitions.



\## Benchmark execution



```bash

bash benchmarks/compare\_2vm.sh pilot

```



To attempt the complete two-VM benchmark:



```bash

bash benchmarks/compare\_2vm.sh full

```



The Part 2 strip solver must also be compiled and available on the participating machines. The benchmark scripts assume the project directories are located under the MPI user's home directory.



The raw logs of the 12 successful benchmark runs are available in `results/final\_benchmarks/`.



\## Experimental limitations



The N = 8192 benchmarks were attempted several times but could not be completed because of excessive execution times and recurring Tailscale connectivity failures.



Experimental performance measurements with P = 8 were not performed because the three available VMs provided only six virtual CPUs in total.



Oversubscription was avoided during performance experiments.



The theoretical analysis includes the missing configurations, but no experimental results are claimed for them.



\## Project structure



```text

part3/

├── src/

│   └── blocks.cpp

├── tests/

├── benchmarks/

├── results/

│   ├── final\_benchmarks/

│   ├── demo\_blocks\_case1.png

│   ├── demo\_blocks\_case2.png

│   ├── demo\_blocks\_caseV.png

│   ├── demo\_verification.txt

│   └── step16\_correctness\_log.txt

├── Makefile

├── .gitignore

├── README.md

└── MPI hostfiles

```



\## Conclusion



The MPI 2D block implementation successfully reproduces the serial reference solution for the tested configurations.



The completed performance experiments demonstrate the potential advantage of 2D block decomposition over 1D strips, while highlighting the impact of network latency and connectivity on distributed performance.

