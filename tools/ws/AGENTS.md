# tools/ws — agent contract

`ws` is the AI Workspace's command-line tool: one C++20 binary. All workspace
features are being standardized into it (decision 2026-10-04: C++, orch plus all
`bin/` tools).

## Startup

1. Read `.ai/STATE.md` (status, exact next action) and `.ai/TASKS.md`.
2. `git -C tools/ws status` — this folder is its own Git repo.
3. Open `.ai/DECISIONS.md` or source files only when the task needs them.

## Build and test

```bash
make -C tools/ws            # → tools/ws/build/ws (first build downloads nlohmann/json, SHA-256 pinned)
make -C tools/ws test       # end-to-end tests with fake CLIs; needs Node for the runner only
tools/ws/build/ws version
```

Run both after every change. Never claim a test passed without running it.

## Rules

- C++20, POSIX, no warnings (`-Wall -Wextra -Wpedantic`). Dependencies: system
  SQLite and the build-time-fetched nlohmann/json only. Never commit `build/` or
  third-party source.
- Behavior and output text must match the end-to-end tests in `test/`; the
  TypeScript original in `projects/agent-orchestrator` is the reference.
- Child sessions: never pass permission-bypass flags (a test enforces this).
- Ports from `bin/` keep the old name as a one-line wrapper calling `ws`.
- Publishing: `bin/export-oss` from the workspace root, then commit and push in
  `projects/ai-workspace` (it pushes as the personal account automatically).

## Layout

`src/util/` process, args, fs, SQLite · `src/orch/` packet, adapters (Claude,
Codex), acp (Kiro), sandbox, registry, worktree, commands · `test/` black-box
tests + `test/fakes/` (fake claude, codex, kiro, srt).
