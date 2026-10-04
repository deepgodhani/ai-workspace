# STATE — tools/ws

## Status (2026-10-04)

- Phase 1 (foundation) and phase 2 (`ws orch` port) **done**: 34/34 end-to-end
  tests pass; real read-only runs OK for Claude, Codex, Kiro (inside srt); real
  srt OS-level block of an out-of-worktree write verified.
- Kiro shell policy hardened: chained / redirected / substituted commands are
  denied unless every segment matches `--allow`.
- `bin/ws` and `bin/orch` in the workspace root call `build/ws`.
- Published 2026-10-04: exported to github.com/deepgodhani/ai-workspace
  (commit 0074490). A fresh copy of the export builds `ws`, passes 34/34 ws
  tests and 18/18 `bin/selftest` (which now builds `ws`).
- Continuity verified: a fresh Claude session and a fresh Kiro session started
  at `~/Workspace` each found the build/test commands and this next action on
  their own (root AGENTS.md → tools/ws/AGENTS.md → .ai/STATE.md).

## Exact next action

Phase 3: port the `bin/` tools into `ws` subcommands, one at a time, each with
end-to-end tests and the old name kept as a wrapper. Order:
`workspace-context` → `new-project` → `new-research` → `new-agent-case` →
`link-skills` → `doctor` → `selftest` → `export-oss` → `token-usage` →
`session-report`. Read the current bash/Python script first; match its output.

## Open decisions

- Test runner stays Node (dev-only) unless the user wants C++ tests.
- `token-usage` / `session-report` keep calling `tokscale` via npx.
