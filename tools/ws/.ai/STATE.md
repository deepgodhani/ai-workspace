# STATE — tools/ws

## Status (2026-10-04, end of Kiro session a34cdf85)

- Phases 1–4 **done** and published: github.com/deepgodhani/ai-workspace
  f6e78a2 (remote `main` verified). The two notes commits after it (9a64d59 and
  the handoff) are not exported yet; code is identical.
- `ws` commands: `context`, `doctor`, `export-oss`, `firecrawl configure|mcp`,
  `link-skills`, `new project|research|agent-case`, `orch`, `selftest`,
  `session-report`, `usage`, `version`. Every bin/ tool is a 3-line wrapper,
  except the private case script (kept bash) and the installers.
- Installers: `bin/setup-ai-workspace-full.sh` is a 209-line bootstrap over the
  export (old version: `.bak`); `bin/init-ai-workspace.sh` unchanged.
- `bin/` needs no `python3` or `rsync`. `export-oss` works from a clone
  (`_shared/oss/` exported).
- Checks last run (2026-10-04): build 0 warnings, 106/106 tests, `bin/selftest`
  18/18, `bin/export-oss --check` clean; same on a copy of the export.
- Private content rule: private names never go into `src/` or `test/` (export
  strips `# private:` blocks only in text files and copies `tools/ws` whole). Put
  them in the bin/ wrapper inside a `# private:` block and pass generic flags (see
  `bin/workspace-doctor`).
- Measurements: `docs/MEASUREMENTS.md` (all sections filled; §7 checklist done).

## Exact next action

Nothing required. Optional, only when the user asks:
1. Sync the export: `bin/export-oss`, review `git -C projects/ai-workspace
   status`/diff, commit there, push (publishes 9a64d59 + this handoff's notes).
2. Re-read the Kiro a34cdf85 row in MEASUREMENTS.md §5 (read before exit: 155
   msgs, $5.93; may have grown slightly).
3. Delete `bin/setup-ai-workspace-full.sh.bak` once the new installer has been used.
4. Decide whether tests move from Node to C++ (open decision).

Porting pattern (if more tools are added):
- Golden run of the old script in a throwaway copy; record output, exit codes,
  side effects, speed. The /tmp/ws-old helpers (bench.py, shims, fuzzers) were
  removed at handoff; recreate as needed.
- Run the new test file against the old script via a `WS_BIN=<shim>`; only
  deliberate deviations may fail (DECISIONS.md).
- Wrapper: 3 lines, `WS_ROOT` = its parent, exec `bin/ws <sub>`; see `bin/selftest`.
- For child output that a script showed directly, use `run_inherit` (util/proc).
- Usage says `ws <sub>`; `-h/--help` prints usage, exits 0.

## Open decisions

- Test runner stays Node (dev-only) unless the user wants C++ tests.
- `usage` / `session-report` keep calling `npx -y tokscale@4.17.0`.
- The bin/ wrappers live outside this repo (the workspace root is not a Git
  repo); they reach GitHub only via `bin/export-oss`.
