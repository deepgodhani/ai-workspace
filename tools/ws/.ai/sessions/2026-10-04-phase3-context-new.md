# 2026-10-04 — phase 3: ws context, ws new

## Done
- `ws context <target>` ← bin/workspace-context (src/context, test/context.test.ts).
- `ws new project|research|agent-case` ← bin/new-project, new-research,
  new-agent-case (src/new, test/new.test.ts).
- The four bin/ scripts are now 3-line wrappers that set `WS_ROOT` and exec `bin/ws`.
- `bin/selftest`: builds `ws` first and unconditionally (the wrappers need it).
  Before this, "workspace-context resolves a target" failed because selftest's
  copy excludes `tools/ws/build` and built `ws` only after that check.

## Checks actually run
- `make -C tools/ws`: no warnings. `make -C tools/ws test`: 53/53. `bin/selftest`: 18/18.
- context: old vs new output identical for 86 real targets and error cases, run
  from /tmp. The original script passes all 9 context tests via a shim.
- new: golden run of the old scripts in a throwaway workspace copy, replayed
  with `ws new`: same stdout/stderr/exit codes (usage name aside), `diff -r`
  identical contents. Only modes differ (old: 0600 after substitution, new: kept).
  The original scripts pass 8/10 new tests via a shim; the 2 failures are the
  mode fix and the ws-only `ws new` dispatcher test.

## Deviations (recorded in DECISIONS.md)
- Template modes kept in agent cases; usage shows `ws <sub>`; `--help` added.
- A missing template now errors before creating anything (from reading the old
  script: `mkdir -p` runs before `cp`, so it would leave an empty folder; not run).
- Files are only rewritten when a placeholder actually changes them.

## Not done
- link-skills, doctor, selftest, export-oss, token-usage, session-report.
- No re-export/push (not requested).
