# STATE — tools/ws

## Status (2026-10-04)

- Phase 1 (foundation) and phase 2 (`ws orch` port) **done**. Phase 3 in
  progress: `ws context` and `ws new project|research|agent-case` ported, their
  bin/ scripts are now wrappers. 53/53 end-to-end tests pass (34 orch +
  9 context + 10 new); `bin/selftest` 18/18.
- Real read-only runs OK for Claude, Codex, Kiro (inside srt); real srt
  OS-level block of an out-of-worktree write verified. Kiro shell policy:
  every chained segment must match `--allow`.
- `bin/ws` and `bin/orch` in the workspace root call `build/ws`.
- Published 2026-10-04 (github.com/deepgodhani/ai-workspace, commit 0074490),
  before phase 3. Not re-exported since.

## Exact next action

Port `bin/link-skills` → `ws link-skills`, then `workspace-doctor` →
`ws doctor`, `selftest` → `ws selftest`, `export-oss` → `ws export-oss`,
`token-usage` / `session-report` → `ws usage` / `ws session-report`.
Read the script fully first; match its output and exit codes.

Porting pattern (used for `context` and `new`):
- Run the old script in a throwaway copy first and record outputs, files, and
  modes (golden run). Then replay with `ws` and `diff -r` trees and logs.
- Run the new test file against the old script through a small shim
  (`WS_BIN=<shim>`); only deliberate deviations may fail (see DECISIONS.md).
- Wrapper: sets `WS_ROOT` to its own parent (old scripts located the workspace
  from their path, not the cwd) and execs `bin/ws <sub>`; see `bin/new-project`.
- Usage says `ws <sub>` instead of `$0`; `-h/--help` prints usage, exits 0.
- `run()` throws when a binary is missing (posix_spawnp); catch it where the
  script tolerated a missing tool.
- `bin/selftest` builds `ws` first (required now; the wrappers depend on it).

## Open decisions

- Test runner stays Node (dev-only) unless the user wants C++ tests.
- `token-usage` / `session-report` keep calling `tokscale` via npx.
- The bin/ wrappers live outside this repo (the workspace root is not a Git
  repo); they reach GitHub only via `bin/export-oss`.
