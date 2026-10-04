# 2026-10-04 — ws link-skills, phase 4 measurement plan (Kiro, same session as context/new)

## Done
- `ws link-skills` ← bin/link-skills (src/skills, test/link-skills.test.ts, 7 tests).
  bin/link-skills is now a wrapper.
- `docs/MEASUREMENTS.md`: phase 3/4 plan with on-paper estimates and an
  "Actual" column per tool (correctness, speed, size/deps, session cost), the
  installer proposal (§6, needs user OK), and the phase 4 definition of done.
- TASKS.md: user added the private case script, configure-firecrawl,
  firecrawl-mcp-wrapper and both installers to scope.
- Published f21f68a (context + new) earlier in this session.

## Checks actually run
- `make -C tools/ws`: 0 warnings. `make -C tools/ws test`: 60/60. `bin/selftest`: 18/18.
- Original bin/link-skills passes 6/7 new tests via a shim; the 1 failure is the
  deliberate fix (no dangling `.kiro/skills/*` link when there are no skills).
- Real workspace: `bin/link-skills` printed 12× `ok`, so no change.
- Speed: link-skills 127 ms → 17.3 ms via wrapper (3.8 ms direct);
  workspace-context 49.1 → 26.5 ms.

## Not done
- doctor, selftest, export-oss, token-usage, session-report, the three extra
  scripts, installers (need approval of MEASUREMENTS.md §6), phase 4.
- Session-cost row for this session: Kiro's local log had not updated
  mid-session; fill in from `bin/token-usage --today --by-session` next time.
- Not exported/pushed since f21f68a.
