# ws — measurements (phase 3/4)

What porting `bin/` into `ws` is expected to change, the on-paper estimate,
and the actual number once measured. Fill in "Actual" as each port lands; never
copy an estimate into it. Machine: macOS, Apple Silicon. Started 2026-10-04.

## 1. What to expect (and what not to)

- **Correctness:** every port must reproduce the old script's output and exit
  codes. This is the main deliverable.
- **Wrapper overhead:** each `bin/` wrapper adds ~13 ms of bash start-up; call
  `bin/ws <sub>` directly where that matters.
- **Speed:** small, only where the script forks many processes or starts
  Python. Tools dominated by network or `npx` will not get faster.
- **Run-time dependencies:** Python 3 and `rsync` drop out of `bin/`.
- **Agent token spend:** the port does not change it. Wrapper output is
  byte-identical, so a call costs the same tokens. Spend here is driven by
  session length (root `docs/MEASUREMENTS.md` §3–4), so section 5 tracks the cost
  of doing the port itself.

## 2. Correctness (per port)

Method: the new `test/<tool>.test.ts` passes against `ws`, and also runs against
the old script through a shim (`WS_BIN=<shim>`). The old script may fail only
the deliberate deviations listed in `.ai/DECISIONS.md`.

| Tool | Tests | Old script passes | Status |
|---|---|---|---|
| workspace-context → `ws context` | 9 | 9/9 | done |
| new-project/research/agent-case → `ws new` | 10 | 8/10 (mode fix, `ws`-only dispatcher) | done |
| link-skills → `ws link-skills` | 7 | 6/7 (no-skills fix) | done |
| workspace-doctor → `ws doctor` | 7 | 4/7 (exec-only PATH, private checks via flags, `--help`) | done; real workspace output byte-identical (98 lines) |
| selftest → `ws selftest` | 5 | 3/5 (env cleared in the copy, `--help`) | done; real run 18/18 |
| export-oss → `ws export-oss` | 9 | 4/9 (all 5: `_shared/oss` now exported; earlier 8/8) | done; real `--check` and export trees byte-identical; fuzz 300 runs, 0 diffs |
| token-usage, session-report → `ws usage`, `ws session-report` | 13 | 12/13 (`--help` to stdout, exit 0) | done; real tokscale output identical; fuzz 900 runs, 0 diffs |
| configure-firecrawl, firecrawl-mcp-wrapper → `ws firecrawl configure`, `ws firecrawl mcp` | 12 | 10/12 (file parsed instead of sourced, `--help`) | done; 13 of 14 scenarios identical to the old scripts (the 14th is the deliberate refusal); real credential file read identically |
| the private case script | – | – | kept as bash (private; DECISIONS.md) |
| setup-ai-workspace-full.sh | – (installer; manual runs) | – | done: 209-line bootstrap over the export; fresh install, rerun, Git-URL source checked |
| init-ai-workspace.sh | – | – | unchanged (separate portable scaffold, no `ws`) |

Target: 100% of tests pass against `ws`; every old-script failure explained.

## 3. Speed: old script vs `ws` (mean wall time)

Method: run each command N times, discarding output, and report the mean in ms. Old
scripts come from `projects/ai-workspace` history (`git show a99291f:bin/<name>`)
placed in a throwaway workspace copy. Baselines: `bash -c true` 2.1 ms,
`python3 -c pass` 18.5 ms, `ws version` 2.4 ms.

| Command | Old (measured) | Estimate for `ws` | Actual `ws` | Why |
|---|---|---|---|---|
| workspace-context tools/ws | 49.1 ms | ~25 ms | **26.5 ms** | fewer forks; git calls remain |
| new-project / new-research | not measured | −20 ms each | – | no `python3` start |
| link-skills | 127 ms (N=20) | <15 ms | **17.3 ms** via wrapper, 3.8 ms direct | no `ln`/`readlink` forks; the bash wrapper costs ~13 ms |
| export-oss --check | 402 ms (N=3); 405.6 ms (N=5) | 150–800 ms | **135.0 ms** via wrapper, 122.4 ms direct (N=5) | hand-written matchers, no `std::regex`; no Python start |
| workspace-doctor (real) | 16.2–16.9 s (N=3, two runs) | ~17 s, unchanged | **16.8 s** via wrapper (N=3) | `claude mcp list` alone is 15.5–16.2 s |
| workspace-doctor (fake ws, fake CLIs, N=30) | 43.8 ms | – | **33.2 ms** via wrapper, 8.5 ms direct | own work only; wrapper costs ~25 ms here |
| token-usage --today --by-session (real) | 727.6 ms (N=3) | unchanged | **725.0 ms** via wrapper (N=3) | `npx tokscale` dominates |
| session-report (real) | 735–871 ms (N=5, two runs) | unchanged | **704–759 ms** via wrapper (N=5) | `npx tokscale` dominates |
| firecrawl-mcp-wrapper, fake npx (N=30) | 25.2 ms | – | **21.2 ms** via wrapper, 6.2 ms direct | startup of the MCP server, before npx |
| token-usage / session-report, fake npx (N=20) | 36.4 / 43.5 ms | – | **7.5 / 7.5 ms** direct | no bash+Python start |
| selftest (real) | 50.0 s (N=3) | – | **49.6 s** via wrapper (N=3) | `make` in the copy ~18 s, `workspace-doctor` ~16 s, rsync copy 2.9 s |

## 4. Size and dependencies

| | Before (2026-10-04) | Estimate after phase 3 | Actual |
|---|---|---|---|
| `bin/` script lines (excl. installers) | 952 | ~40 (wrappers) | 95 = 52 wrapper lines + 43 (the private case script, kept as bash) |
| Installer lines (`setup-ai-workspace-full.sh` + `init-ai-workspace.sh`) | 6,064 (188 KB) | see §6 | 830 (209 + 621) |
| `ws` C++ lines added / test lines added | – | ~2,500 / ~2,000 | 2,342 / 1,178 since 3965aff (src / test; phase 3 so far) |
| `bin/` tools needing `python3` | 6 | 0 | 0 |
| `bin/` tools needing `rsync` | 2 | 0 | 0 |
| `ws` binary size | 0.80 MB | ~1.2 MB | 1.06 MB |

## 5. Cost of doing the port (agent sessions)

Method: `bin/token-usage --today --by-session` at the end of each session; record
the session row. Costs are API list-price estimates from local logs, not invoices.
Kiro logs record no cache reads, so Kiro rows are not comparable with Claude rows.

| Session | Work | Messages | Tokens | Tokens/msg | Cost (list) |
|---|---|---|---|---|---|
| Claude 8d29c6e1 | phases 1–2, publish (one long session) | 245 | 121.0M | ~494k | $38.94 |
| Claude subagent sessions (4, today) | helpers, not attributed | 121 | 40.2M | ~332k | $9.40 |
| Kiro 13be2bb6 | phase 3: context, new, publish (to msg 65) | 65 | 0.74M* | – | $2.96 |
| Kiro 13be2bb6 (cont.) | link-skills, this plan (msgs 66–104) | 39 | 0.66M* | – | $2.68 |
| Kiro 13be2bb6 (total) | read in the next session: 104 msgs, 1.40M, $5.64; later the same day it showed 108 msgs, 1.75M, $7.02, so the log was still growing | 108 | 1.75M* | – | $7.02 |

| Kiro a34cdf85 (this session) | doctor, selftest, export-oss, usage, session-report, firecrawl | 113 so far (log lags; re-read after exit) | 0.71M* so far | – | $2.86 so far |

*Kiro does not record cache reads.

**On paper, remaining phase 3 + 4.** Work left is ~640 script lines, `ws init`
for the installers (§6), and the phase 4 docs. At this session's rate (~310
lines in ~65 messages), that is roughly **300–400 messages**. Using the per-message re-read
from the root measurements (~140k tokens in sessions under 100 messages, ~430k in
400–800-message sessions) and the blended price seen above (~$0.32 per million
Claude tokens):

| Plan | Tokens | Cost estimate |
|---|---|---|
| One long session (~350 msgs × ~430k) | ~150M | ~$48 |
| ~5 short sessions (~70 msgs each × ~140k) + restarts | ~50M | ~$17 |

That is about 65% less with handoffs. **This is a model, not a result.** Fill in
section 5 rows per session to check it.

## 6. Installers (decided 2026-10-04: bootstrap over the export, not `ws init`; see DECISIONS.md)

`setup-ai-workspace-full.sh` (5,443 lines, 93 embedded files) and
`init-ai-workspace.sh` (621 lines) run *before* `ws` exists, on a machine that
may lack a C++ compiler. A line-for-line port cannot work, because a wrapper
cannot exec a binary that has not been built yet. Proposal for the next session:

- Keep a short bash bootstrap: install dependencies, then `make -C tools/ws`.
- Move the rest into `ws init`, using the exported repository as the template
  instead of 93 heredocs.

Estimate: ~5,400 lines removed, and the two installers and the export can no
longer drift apart. Needs the user's OK.

## 7. Phase 4 checklist (definition of done)

- [x] Sections 2–4 have an "Actual" for every tool, installers included. 2026-10-04.
- [ ] Section 5 has a row for every porting session (a34cdf85 still partial: Kiro's
      log lags; re-read after exit), compared against the model.
- [x] Root `docs/MEASUREMENTS.md` §1a added with the `ws` results (user said "Go
      phase 4"). 2026-10-04.
- [x] README (`_shared/oss/README.md`), HELP, WORKSPACE, `tools/ws/README.md` list
      the `ws` commands; `bin/SHARING.md` marked out of date. 2026-10-04.
- [x] Fresh copy of the export (to /tmp): clean build with 0 warnings, 105/105 `ws`
      tests, `bin/selftest` 18/18, `bin/workspace-doctor` exit 0; no private names
      in the export. 2026-10-04.
- [ ] Final export to `projects/ai-workspace` + push (only when the user asks).

Fixed 2026-10-04: `bin/export-oss` failed in every clone because `_shared/oss/` was
never exported; it is now (DECISIONS.md), and a test exports a clone again.
