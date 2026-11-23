# Network conditions documented by the team

- **Date of current route tests:** 2026-11-23 (UTC timestamps in the screenshots).
- **Host connectivity:** all three Windows laptops on shared Wi-Fi, as explicitly confirmed by the team.
- **VM networking during 2026-11-23 route checks:** VirtualBox **Bridged Adapter** on all three VMs, as confirmed by the team.
- **Tailnet:** shared team Tailscale network; each VM's Tailscale address remains in the 100.x.x.x space.
- **Route status during the current check:** all six directions use **direct LAN endpoints**, not `DERP(...)`.
- **Local LAN endpoints on 2026-11-23:** heat-1 `192.168.44.118`, heat-2 `192.168.44.117`, heat-3 `192.168.44.172`.
- **Original full 22-size MPI ping-pong benchmarks:** carried out on **2026-11-21**, using Wi-Fi with VirtualBox Bridged Adapter and direct paths observed for the previously checked peers. This evidence is not a re-run of those benchmarks.
- **Older experiments:** VirtualBox NAT led to Tailscale DERP relaying and incomplete/very slow cross-VM benchmarks. Those results are archived under `part0/results/raw/prebridge_incomplete/`.
- **Interpretation:** the 2026-11-23 direct routes confirm the later network mode and complete the missing per-pair route documentation. They do **not** prove every packet of the 2026-11-21 benchmarking followed the same route. Tailscale can change routes as network conditions change.

All machine/route evidence in this folder is supported by team screenshots or team confirmation; no rerun has been performed to generate this package.
