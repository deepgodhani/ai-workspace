---
name: handoff
description: Prepare a compact, accurate handoff before ending a development project, reusable-agent verification case, or research session. Use when the user says handoff, finish session, end session, save context, or prepare for a fresh session.
---

# Prepare session handoff

Determine whether this is a reusable agent, development project, or research
topic. When the directory is under `~/Workspace/agents/`, use reusable-agent
mode before considering the generic `.ai/STATE.md` project rule.

Do not commit changes unless the user explicitly asks.

## Reusable-agent handoff

Use this mode inside `~/Workspace/agents/<agent>/`.

1. Identify the selected case from the task and `.ai/STATE.md`. Do not scan
   every case.
2. Read the selected case's `verification.yaml`, `behavior-catalog.yaml`, and
   `invariants.yaml`. Read only redacted runtime summaries needed for status;
   never load `.env` or raw `inputs/` by default.
3. Inspect Git status/diff only when the agent directory is a Git repository.
   Never run Git as if the workspace root were the repository.
4. Update the agent's `.ai/STATE.md`, `.ai/TASKS.md`, and `.ai/INDEX.md with:
   - Agent objective and selected case
   - Implemented or completed stages
   - Stage verdicts with evidence paths
   - Checks actually run
   - Blocked and unverified stages
   - Port roles and environment-variable names, never values
   - Exact next action and required approval
5. Update `.ai/DECISIONS.md` only for durable framework, case, safety, billing,
   or port-allocation decisions.
6. Create a dated `.ai/sessions/` note for meaningful framework or case work.
7. If a port assignment changed, ensure
   `tools/database-port-registry.yaml` reflects ownership without credentials.
8. If application files changed in `projects/`, prepare a separate handoff in
   that owning repository; do not hide project changes inside the agent
   handoff.
9. Report:
   - Selected case
   - Verified, failed, blocked, and unverified stages
   - Files and redacted evidence changed
   - Checks actually run
   - Remaining approval or endpoint requirement
   - Exact next action
   - Suggested Git commit message only when a Git repository exists

## Development project handoff

Use this mode when `.ai/STATE.md` exists.

1. Inspect `git status --short` and `git diff --stat` from the owning
   repository.
2. Update `.ai/STATE.md` with:
   - Current objective
   - Work completed
   - Current status
   - Verified behavior
   - Checks and tests actually run
   - Relevant files
   - Blockers and uncertainties
   - Exact next action
3. Update `.ai/TASKS.md`:
   - Mark genuinely completed work
   - Keep unfinished work active
   - Add newly discovered tasks
4. Update `.ai/DECISIONS.md` only for durable technical decisions.
5. Create a dated note under `.ai/sessions/` when the session involved
   meaningful work, investigation, failures, or important test results.
6. Keep `.ai/STATE.md` compact. Do not place transcripts or large logs in it.
7. Report:
   - Files changed
   - Tests actually run
   - Remaining blocker
   - Exact next action
   - Suggested Git commit message

## Research handoff

Use this mode when `SCOPE.md` and `STATE.md` exist.

1. Inspect `git status --short` and `git diff --stat`.
2. Update `STATE.md` with:
   - Current research objective
   - Work completed
   - Sources or documentation sections processed
   - Important findings
   - Remaining gaps
   - Exact next research action
3. Update `COVERAGE.json` when URL discovery, mapping, crawling, or
   processing changed.
4. Update `SOURCES.md` when authoritative sources were added.
5. Update `GAPS.md` for failures, contradictions, inaccessible material,
   and unresolved questions.
6. Put detailed findings in `REPORT.md`.
7. Keep `CONTEXT.md` compact and reusable by future sessions.
8. Do not claim complete coverage without measurable evidence.
9. Report:
   - Files changed
   - Coverage change
   - Important new findings
   - Remaining gaps
   - Exact next action
   - Suggested Git commit message

## Accuracy rules

- Never record a test, crawl, command, or verification that was not run.
- Never turn an offline snapshot pass into a complete live migration pass.
- Never record a port as live merely because it is reserved in the registry.
- Clearly distinguish facts, inferences, and unresolved questions.
- Never copy credentials, `.env` content, raw case inputs, or database records
  into handoff files.
- Do not copy full chat transcripts into context files.
- Do not commit automatically.
