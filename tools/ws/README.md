# ws — AI Workspace command-line tool

One C++20 binary for workspace features. Build once:

```bash
make            # downloads nlohmann/json 3.11.3 (SHA-256 pinned) into build/, links system SQLite
make test       # end-to-end tests with fake claude/codex/kiro/srt (needs Node only for the test runner)
```

`bin/ws` and `bin/orch` in the workspace root call `build/ws`.

## Commands

- `ws context <target>` — resolve a target (path or bare name) and print its type,
  Git state, AGENTS.md chain, and startup-context sizes. `bin/workspace-context`
  calls it.
- `ws new project|research|agent-case …` — scaffold from `_shared/templates/…` or
  `agents/<agent>/cases/_template` and fill placeholders. `bin/new-project`,
  `bin/new-research`, `bin/new-agent-case` call it.
- `ws orch …` — scoped AI CLI child sessions for Claude Code, Codex, and Kiro:
  spawn (read-only or `--write` in a Git worktree), resume, merge results across
  CLIs, list/show/diff/close. Sandboxed by default (`--sandbox auto`: Claude/Codex
  native, Kiro inside Anthropic's sandbox-runtime). See `ws orch --help`.
- `ws version`

More workspace commands are being ported from `bin/` (bash/Python) into `ws`.

## Requirements

C++20 compiler (clang or gcc), POSIX, SQLite 3 (macOS built-in; `libsqlite3-dev` on
Debian/Ubuntu), `curl` for the first build, `git`. Optional at run time: `npx` for
the srt sandbox tier and for `tokscale`-based usage reports.
