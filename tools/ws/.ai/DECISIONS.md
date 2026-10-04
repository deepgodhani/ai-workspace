# DECISIONS — tools/ws

- 2026-10-04 — **C++20 for all workspace tools** (user decision; Go/Rust were
  offered). One binary, `ws`, with subcommands; `bin/` names stay as wrappers.
- 2026-10-04 — **nlohmann/json fetched at build time, SHA-256 pinned** (matches
  the release notes); never vendored. SQLite from the system.
- 2026-10-04 — **Black-box tests carried over from the TypeScript version** as the
  spec; Node is a dev-only dependency for the runner.
- 2026-10-04 — **Shell allow-list matches every chained segment**; substitution
  and redirection always denied (found by a real Kiro run).
- 2026-10-04 — **Ports keep output and exit codes, but fix accidental side
  effects.** `ws new agent-case` keeps template file modes; the bash script left
  every substituted text file at 0600 (mktemp + mv). Usage lines say `ws <sub>`
  instead of `$0`; `-h/--help` added. A missing `git` is tolerated as before.
