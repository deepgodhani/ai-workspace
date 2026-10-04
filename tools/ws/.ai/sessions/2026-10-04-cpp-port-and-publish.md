# 2026-10-04 — C++ port of orch, publish, continuity check

## Done
- Foundation: `src/util` (posix_spawn processes with stdin/timeouts, streaming
  child for JSON-RPC, strict arg parser, fs/workspace discovery, SQLite).
  Makefile fetches nlohmann/json 3.11.3, SHA-256 checked against release notes.
- `ws orch` ported from TypeScript with output parity; registry is compatible
  with the TypeScript one (reads its existing sessions).
- Test suite ported (black-box, Node runner + fake claude/codex/kiro/srt); hidden
  `orch _decide` / `_parse` replace the TypeScript unit imports.

## Checks actually run
- `make -C tools/ws` from an empty `build/` (no warnings); `make -C tools/ws test`
  34/34; `bin/selftest` 18/18; fresh copy of the export: build + 34/34 + 18/18.
- Real runs via the C++ binary: Claude read ($0.057, native sandbox), Codex read,
  Kiro read (0.11 credits, inside srt); Kiro write with `--allow "Bash(touch:*)"`:
  in-worktree touch succeeded, out-of-worktree touch blocked by srt
  ("Operation not permitted").
- Fresh-session discovery at `~/Workspace`: Claude (`claude -p`, read-only, $1.19
  with full user setup loaded) and Kiro (ACP, 0.24 credits) both answered with
  the right build/test commands and next step.

## Found and fixed
- Kiro shell allow-list matched by prefix, so `touch a && other` was approved.
  Now every chained segment must match; `$(...)`, backticks, redirects and
  newlines are always denied. C++ also needed a trailing-separator check
  (`touch a &&`) because std::sregex_token_iterator drops a trailing empty
  segment. Same fix applied to the TypeScript reference.
- `tools/ws/AGENTS.md` commit-identity bullet is marked private (export scanner
  blocked it).

## Not done
- Phase 3 (port bin/ tools) and phase 4 not started.
