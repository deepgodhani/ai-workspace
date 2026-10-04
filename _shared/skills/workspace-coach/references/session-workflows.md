# Session Workflows

## Reusable-agent session

Activate `agents/<agent>/` from the workspace root or from the agent directory.
Read applicable `AGENTS.md`, compact agent state, and the selected case
`STATUS.md` and manifest. Read behavior catalogs, invariants, and redacted
runtime summaries only when required. Do not load `.env`, raw inputs, exports,
or every case. Inspect Git only when the agent belongs to a repository. End
with stage verdicts, evidence paths, approvals, and one exact next action.

## Project session

Activate the project from the workspace root or project directory. Read
applicable `AGENTS.md`, `.ai/STATE.md`, `.ai/TASKS.md`, and `.ai/INDEX.md`;
inspect repository Git status; continue the exact next action. End by updating
compact state and one dated session note when detail is useful.

## Research session

Activate the topic from the workspace root or research directory. Read
applicable `AGENTS.md`, `SCOPE.md`, `STATE.md`, `CONTEXT.md`, and `GAPS.md`;
inspect coverage when crawling; write only in the current topic. End with
measurable coverage and an exact next action.

## Handoff

Use `$handoff` before changing agents or ending an unfinished session. Record
only checks that actually ran. For verifier work, keep
`PASS`/`FAIL`/`BLOCKED`/`UNVERIFIED` distinct and prepare a separate project
handoff when application files changed. Do not copy full chat transcripts,
credentials, case inputs, or raw records into memory.
