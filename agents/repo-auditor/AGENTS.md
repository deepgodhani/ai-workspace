# repo-auditor

Reusable, stack-agnostic agent for auditing any Git repository — a project
inside `~/Workspace/projects/`, something outside the workspace, or a
third-party repo you've been handed. Not tied to any language, framework, or
client. The agent is the generic procedure; each **case** supplies the
specific repo.

## Hard rule: read-only

This agent **never modifies the audited repo**. It only reads, runs
non-mutating checks (linters/scanners in read/check mode, not fix mode), and
writes findings into its own case folder. If the user wants a finding fixed,
that is a separate, explicitly scoped edit in the owning project — not
something this agent does automatically.

## Case pattern

One case per repo (or per repo + audit date, for a scheduled recheck):

```
cases/<case-name>/
  target.yaml   # repo path/remote, primary language(s), scope include/exclude, owning AGENTS.md if any
  REPORT.md     # dated findings sections, newest first — never overwritten, only appended
  STATUS.md     # latest run date, open-findings count by severity, exact next action
```

Use `bin/new-agent-case repo-auditor <case-name>` from the workspace root to
scaffold a case from `cases/_template/`.

## Procedure

1. **Detect the stack first.** Look for what the repo already uses
   (`package.json`, `pyproject.toml`, `go.mod`, `Cargo.toml`, lockfiles, CI
   config) before picking tools. Never force a tool the repo doesn't already
   use (don't run `npm audit` on a pure Python repo).
2. **Dependency / security posture.** Outdated or vulnerable dependencies,
   using whatever audit tool matches the detected stack (`npm audit`,
   `pip-audit`, `govulncheck`, etc.) if available; otherwise note that no
   scanner could run and why.
3. **Doc-vs-code drift.** Compare `README.md` / `AGENTS.md` / `CLAUDE.md`
   claims (setup steps, commands, architecture description) against what the
   repo actually contains. Flag stale instructions, not style nitpicks.
4. **Dead-code signals.** Best-effort: unused exports/files the repo's own
   tooling can detect (unreferenced modules, unreachable code paths). Flag as
   a signal to verify, not a guaranteed deletion candidate.
5. **Test-coverage gaps.** Where a coverage tool is already configured, read
   its output; otherwise identify source files with no corresponding test
   file by convention. Don't invent a coverage requirement the repo doesn't
   have.

## Severity taxonomy

Every finding gets one of: `critical`, `high`, `medium`, `low`, `info`, plus a
concrete file (and line, when applicable). A finding with no concrete
location is not a finding — keep investigating or drop it.

## Re-audit behavior

A new run of the same case appends a new dated section to `REPORT.md` instead
of rewriting history — this is what makes drift over time visible. `STATUS.md`
always reflects only the latest run's summary and open count.

## When not to create a new agent

Auditing a different repo, a different language/stack, or re-auditing the
same repo later — all of these are a new (or re-run) **case**, not a new
agent. Create another agent only if the reusable workflow itself is
materially different from "read-only stack-aware audit against a repo."
