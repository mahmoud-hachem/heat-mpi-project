# AGENTS.md: AI assistance in this project

This file records which AI assistants worked on this repository, what they
did, and the rules they had to follow. The complete conversations are in
[`llm_log/`](llm_log/README.md).

## Assistants used

| Assistant | Interface | Main use |
| --- | --- | --- |
| ChatGPT | Web chat | Explaining concepts, planning, cluster and Tailscale troubleshooting, early drafts |
| OpenAI Codex | Codex Desktop, in this repository (24–28 Nov 2026) | Checking each part against the requirements, code fixes and finalising Parts 1–3, demo guides, drafting the Word report sections for Parts 0–4, guiding the 28 Nov hotspot benchmark runs |
| Claude Code | Claude desktop app, Code tab, in this repository (28 Nov 2026) | Part 4 hotspot campaigns and analysis, Part 0 hardware survey, report fixes, summary, slides, `llm_log/`, this file |

The team ran every command on the VMs. The assistants could not reach the VMs
directly: the team pasted the terminal output into the chat, and every
measurement in this repository comes from those real runs.

## Rules for any agent working here

1. **Never fabricate measurements.** Numbers in READMEs, the report or the
   slides must come from files in `part*/results/`.
2. **Never pool networks.** Each campaign records `network=` in `meta.txt`
   (`home-wifi-nov`, `cafe-relay-28nov`, `hotspot-28nov`). Analyses and
   tables use one network at a time.
3. **Keep failures.** Failed, stalled or interrupted runs stay in the results
   folders as evidence. The benchmark scripts refuse to overwrite existing
   results.
4. **Do not change working solver code without evidence.** Every parallel
   version must stay bit-identical to the serial output (`tests/check_correctness.sh`).
   Build with `-O2 -ffp-contract=off`.
5. **No secrets anywhere.** Do not put passwords, SSH private keys, Tailscale
   auth keys or tokens in code, reports, commits or AI prompts. The `mpi`
   account has no sudo rights; do not try to get them.
6. **Same path on all VMs.** Build on each VM. Open MPI does not copy binaries.
7. **Stopping runs.** `Ctrl+C` does not stop `timeout … mpirun` campaigns.
   Kill them from a second SSH session (`pkill -f run_campaign; pkill mpirun`
   on every host).
8. **Commits** are made only when a team member approves them.

## Conventions

- C++17 with Open MPI. Python 3 with numpy/matplotlib for analysis.
- Cases: `1` (all edges at 1), `2` (top and bottom at 1), `V`
  (sin·sin, has an exact solution).
- Performance model: compute time = owned points × t_c(VM). Communication
  time = Σ (t_s + m·t_w) over the real neighbour links. The slowest rank
  sets the step time.
