# Part 0 supplementary evidence

This folder is intended to be placed **inside `heat-mpi-project/part0/`**. It holds material collected during the team's 2026-11-23 follow-up evidence session and is **additional to**, not a replacement for, the original November 21 benchmark data in `results/`.

## hardware/

- `heat[1-3]_hardware_screenshot.*`: original Ubuntu terminal screenshots of version, CPU, RAM, architecture and Open MPI checks.
- `heat[1-3]_windows_cpu_screenshot.*`: original Windows CPU-console screenshots extracted unchanged from the team's supplied Word report.
- `heat[1-3]_hardware.txt`: clearly labeled **transcriptions** from the Ubuntu screenshots, also including separately team-confirmed VirtualBox allocations and Windows host CPU models. These are not falsely represented as original terminal files.
- `vm_hardware_summary.csv`: machine-readable hardware comparison.

## network/

- `heat[1-3]_tailscale_screenshot.*`: original screenshots of the per-peer Tailscale direct checks.
- `network_heat[1-3].txt`: clearly labeled **transcriptions** of those screenshots, not the exact Ubuntu-produced files.
- `network_summary.csv`: six one-direction observations (two per VM), with source timestamps and values.
- `network_conditions.md`: separates network conditions observed on Nov 23 from the original benchmark run on Nov 21.

### Provenance and caveats

The `.txt` files are reconstructed from uploaded screenshots; the original machine-created files remain on the corresponding VMs under `/home/mpi/part0_final_evidence/` unless moved. For the strictest raw-data preservation, replace the transcriptions with those original files before committing if available, and keep the screenshots.

The three screenshots from Windows disclose physical CPU models; the other screenshots show machine names, private Tailscale and LAN IP addresses, and timestamps. **Review before publishing to a public GitHub repository.**

The team confirmed 2 vCPUs and 4096 MB VirtualBox RAM allocation, and Wi-Fi/Bridged Adapter network conditions, in the chat. Those settings are not independently backed by VirtualBox settings screenshots in this package. Guest-visible 3.8 GiB is consistent with, but does not itself prove, 4096 MB allocated RAM.
