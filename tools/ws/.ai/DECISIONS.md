# DECISIONS — tools/ws

- 2026-10-04 — **C++20 for all workspace tools** (user decision; Go/Rust were
  offered). One binary, `ws`, with subcommands; `bin/` names stay as wrappers.
- 2026-10-04 — **nlohmann/json fetched at build time, SHA-256 pinned** (matches
  the release notes); never vendored. SQLite from the system.
- 2026-10-04 — **Black-box tests carried over from the TypeScript version** as the
  spec; Node is a dev-only dependency for the runner.
- 2026-10-04 — **Shell allow-list matches every chained segment**; substitution
  and redirection always denied (found by a real Kiro run).
