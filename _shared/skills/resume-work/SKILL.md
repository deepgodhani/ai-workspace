---
name: resume-work
description: Resume the current development project, reusable-agent verification case, or research topic using file-based context. Use when the user says continue, resume, start work, continue verification, continue research, or asks what to do next.
---

# Resume current work

Determine the current workspace mode from the files in the working
directory.

## Reusable-agent mode

Use this mode first when the resolved working directory is inside
`~/Workspace/agents/<agent>/`.

Before substantive work:

1. Read the agent's `AGENTS.md`, `.ai/STATE.md`, `.ai/TASKS.md`, and
   `.ai/INDEX.md`.
2. Read `README.md` and only the architecture/quick-start section relevant to
   the next action.
3. Identify one selected case from the user request or current state.
4. Read that case's `verification.yaml`, `behavior-catalog.yaml`, and
   `invariants.yaml`.
5. Read `tools/database-port-registry.yaml` only when endpoint allocation or
   identity matters.
6. Read redacted `.runtime/` stage summaries only when required. Do not load
   `.env`, credential values, raw `inputs/`, or every case.
7. Inspect Git status only when the agent directory is a Git repository.
8. Run `mfv doctor` when current file, environment-variable-presence, or port
   readiness matters. Do not authenticate merely to resume context.

Briefly state:

- Reusable agent and selected case
- Current stage verdicts
- Important blocker or required approval
- Exact next action

Continue from the exact next action unless the user supplied a different task.
Keep application repositories read-only unless application changes are
explicitly in scope.

## Development project mode

Use project mode when `.ai/STATE.md` exists and the directory is not under
`~/Workspace/agents/`.

Before doing substantive work:

1. Read `AGENTS.md`.
2. Read `.ai/STATE.md`.
3. Read `.ai/TASKS.md`.
4. Read `.ai/INDEX.md`.
5. Run `git status --short`.
6. Retrieve additional documents only when relevant.
7. Do not read all files under `.ai/sessions/`.
8. Do not load complete research reports when a compact context file exists.

Briefly state:

- Current objective
- Current status
- Important blocker, if any
- Exact next action

Then continue from the exact next action unless the user supplied a
different task.

## Research mode

Use research mode when `SCOPE.md` and `STATE.md` exist.

Before doing substantive work:

1. Read `AGENTS.md`.
2. Read `SCOPE.md`.
3. Read `STATE.md`.
4. Read `CONTEXT.md` when it contains existing findings.
5. Read `GAPS.md`.
6. Run `git status --short` from the repository.
7. Inspect `COVERAGE.json` when the next task concerns website coverage.
8. Open detailed reports, sources, or raw material only when needed.
9. Never load the complete `raw/` directory into context.

For active research, source refreshes, validation, or unresolved external claims,
automatically use `$local-web-research`. Do not ask the user to choose between web
research and model knowledge. Use model knowledge to form queries, hypotheses, and
interpretations; use retrieved web evidence to verify externally checkable claims,
detect newer context, and provide citations. Never present model memory alone as a
source.

When `STATE.md` marks the topic complete, preserve the completed work. If the user
asks to continue or update it, run a targeted freshness-and-gap search rather than
blindly repeating the previous crawl.

Briefly state:

- Current research objective
- Completed coverage
- Important gaps
- Exact next action

Then continue from the exact next action unless the user supplied a
different task.

## Safety

- Never assume tests or checks passed unless they were actually run.
- Never infer that a registered port is live or belongs to a case without
  checking the selected case and current readiness.
- Do not enable cloud reads, model calls, or database writes merely because a
  previous session discussed them; require the current workflow's gates.
- Do not commit changes unless the user explicitly asks.
- Do not modify unrelated agents, cases, projects, or research topics.
- Do not expose credentials or secrets.
