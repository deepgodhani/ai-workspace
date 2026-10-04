# 2026-10-04 — ws doctor, selftest, export-oss (Kiro)

## Done
- Filled the Kiro 13be2bb6 session-cost row (MEASUREMENTS.md §5).
- `ws doctor` ← bin/workspace-doctor (35da05c). 7 tests; old script 4/7 (exec-only
  PATH check, private checks as flags, `--help`). The private checks are in the
  wrapper's `# private:` block. Real workspace: byte-identical output (98 lines).
- `ws selftest` ← bin/selftest (07b00af). 5 tests; old 3/5 (WS_ROOT/ORCH_WORKSPACE
  cleared inside the copy, `--help`). rsync replaced by an in-process copy.
- `ws export-oss` ← bin/export-oss (901affe). 8 tests; old 8/8. Real workspace:
  identical `--check` output and identical export trees + modes (fresh DEST and a
  dirtied DEST). Differential fuzz: 300+ runs, 0 differences.
- Found: the published export-oss strips its own `MD_BLOCK` regex (DECISIONS.md).
- Added `run_inherit` and `find_on_path` to util/proc.

## Checks actually run
- `make -C tools/ws`: 0 warnings. `make -C tools/ws test`: 80/80. `bin/selftest`: 18/18
  (after every port).
- `bin/export-oss --check` only (via old copy and new wrapper); no real export, no push.
- Speed: doctor 16.8 s real (unchanged, `claude mcp list` 15.5–16.2 s); own work
  43.8 → 8.5 ms. selftest 50.0 → 49.6 s. export-oss --check 405.6 → 135.0 ms.

## Not done
- token-usage, session-report, configure-firecrawl, firecrawl-mcp-wrapper,
  the private case script (needs a decision), installers (§6 approval), phase 4.
- This session's cost row: Kiro's log lagged; fill from `bin/token-usage` next time.

## Continued (same Kiro session, after "continue")
- `ws usage` / `ws session-report` ← bin/token-usage, bin/session-report (aff722f).
  13 tests; old scripts 12/13 (`--help`). Real tokscale output identical; fuzz
  900 runs, 0 diffs (it found two bugs: empty-string sessionId truthiness and
  int/int division above 2^53; both fixed). `bin/` needs no python3 or rsync now.
- Checks: build 0 warnings, 93/93 tests, `bin/selftest` 18/18.
- Speed: unchanged in real use (npx ~0.7 s); own work 36–44 → 7.5 ms.
- Stopped before the Firecrawl scripts: they handle the API key and edit real MCP
  config; asked the user about the credential format and the private case script.

## Continued again (user: "complete all")
- `ws firecrawl configure|mcp` ← bin/configure-firecrawl, bin/firecrawl-mcp-wrapper.
  12 tests; old 10/12 (file parsed, not sourced; `--help`). 13 of 14 golden
  scenarios identical; real credential file read identically (key length only).
- the private case script kept as bash (recommendation accepted).
- Checks: build 0 warnings, 105/105 tests, `bin/selftest` 18/18, `bin/export-oss --check` clean.
- Phase 3 now waits only on the installers (§6 approval).

## Phase 4 (user: "Go phase 4")
- Docs: `tools/ws/README.md` command table; `_shared/oss/README.md` (build ws before
  link-skills, wrappers, C++ not stripped), HELP.md §4 and tree, WORKSPACE.md,
  `bin/SHARING.md` (installer out of date), root `docs/MEASUREMENTS.md` §1a.
  Backups of the root docs: /tmp/ws-phase4-backup (the workspace root has no Git).
- Private script name removed from exported tools/ws files (tests use `bin/private-tool`).
- Fresh copy (export to /tmp): 0 warnings, 105/105, selftest 18/18, doctor exit 0.
- Not done: export to projects/ai-workspace and push (needs the user's ask).
