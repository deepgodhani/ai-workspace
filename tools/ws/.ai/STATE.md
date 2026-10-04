# STATE — tools/ws

## Status (2026-10-04)

- Phase 1, phase 2 (`ws orch`), phase 3 (all bin/ tools except installers) and
  phase 4 docs/fresh-copy check **done**; publish pending. Ported: `ws context`, `ws new project|research|agent-case`, `ws link-skills`,
  `ws doctor`, `ws selftest`, `ws export-oss`, `ws usage`, `ws session-report`,
  `ws firecrawl configure|mcp` ported; their bin/ scripts are wrappers.
  the private case script stays bash (private). 105/105 end-to-end tests
  (34 orch + 9 context + 10 new + 7 link-skills + 7 doctor + 5 selftest +
  8 export-oss + 13 usage + 12 firecrawl); `bin/selftest` 18/18. Only the
  installers remain in phase 3.
- `bin/` no longer needs `python3` or `rsync`.
- Private content rule: private names never go into `src/` or `test/` (export
  strips `# private:` blocks only in text files and copies `tools/ws` whole). Put
  them in the bin/ wrapper inside a `# private:` block and pass generic flags (see
  `bin/workspace-doctor`).
- The published export-oss (f21f68a) is broken: its `MD_BLOCK` regex stripped
  itself. Fixed by the port; takes effect at the next export (not run).
- Phase 4 plan and measurements: `docs/MEASUREMENTS.md`.
- Published: github.com/deepgodhani/ai-workspace commit f21f68a (context + new
  only). Not exported since.

## Exact next action

Phase 4 is done except the publish step (MEASUREMENTS.md §7):
1. When the user asks: `bin/export-oss` (to projects/ai-workspace), review
   `git -C projects/ai-workspace status`/diff, commit there, push. This also
   repairs the published export-oss (f21f68a strips its own regex).
2. Update the Kiro a34cdf85 row in MEASUREMENTS.md §5 from
   `bin/token-usage --today --by-session`.
3. Installers: only after the user approves a plan. Recommended (2026-10-04,
   not yet approved): replace setup-ai-workspace-full.sh with a short bootstrap
   that installs the exported repo (keep a .bak), and keep init-ai-workspace.sh
   as bash plus `make -C tools/ws` and `ws doctor`.
4. Known gap: `bin/export-oss` in a clone fails (no `_shared/oss/`); ask the user.

Porting pattern:
- Copy the old script to `/tmp/ws-old/` first (shims run it from there). Golden run
  in a throwaway copy; record output, exit codes, side effects, speed
  (`python3 /tmp/ws-old/bench.py N cmd…` — recreate if /tmp was cleared).
- Run the new test file against the old script via a shim that copies it to
  `$WS_ROOT/bin/` and execs it (`WS_BIN=<shim>`); only deliberate deviations may
  fail (DECISIONS.md).
- Wrapper: 3 lines, `WS_ROOT` = its parent, exec `bin/ws <sub>`; see `bin/selftest`.
- For child output that a script showed directly, use `run_inherit` (util/proc).
- Usage says `ws <sub>`; `-h/--help` prints usage, exits 0.

## Open decisions

- Test runner stays Node (dev-only) unless the user wants C++ tests.
- `token-usage` / `session-report` keep calling `tokscale` via npx.
- The bin/ wrappers live outside this repo (the workspace root is not a Git
  repo); they reach GitHub only via `bin/export-oss`.
- Installers: MEASUREMENTS.md §6 awaits approval.
