---
name: temp-session
description: Run a disposable cross-workspace task from a folder under ~/Workspace/temp-sessions. Read explicitly selected agents or verification cases, projects, research topics, knowledge notes, and local files as external context, while writing all generated files only inside the current temporary folder.
---

# Temporary Cross-Workspace Session

Use this skill only when the current working directory is inside:

`~/Workspace/temp-sessions/`

## Purpose

Perform an isolated temporary task using selected external context.

The current temporary folder is writable.

All selected external agents/cases, projects, research topics, knowledge notes,
and local paths are read-only unless the user explicitly changes this rule.

## Start checks

Before working:

1. Resolve the current directory using `pwd -P`.
2. Confirm it is inside `~/Workspace/temp-sessions/`.
3. If it is outside that folder, stop and explain the required location.
4. Read files already present in the current temporary folder.
5. Inspect only external sources explicitly named by the user.
6. Do not scan the complete workspace.

## Source naming

Resolve source names as follows:

* `project:<name>`
  `~/Workspace/projects/<name>/`

* `agent:<name>`
  `~/Workspace/agents/<name>/`

* `case:<agent>/<case>`
  `~/Workspace/agents/<agent>/cases/<case>/`

* `research:<name>`
  `~/Workspace/research/topics/<name>/`

* `knowledge:<path>`
  `~/Workspace/knowledge/<path>`

* `local:<absolute-path>`
  Use the exact supplied path.

Natural-language source names are also acceptable.

## External context rules

Treat selected external folders as read-only.

Never modify external:

* Application source code
* Agent code, `.ai/` files, cases, or runtime evidence
* Project `.ai/` files
* Research files
* Knowledge notes
* Git branches or commits
* Configuration files
* Environment files
* Credentials
* Datasets

Never run Git write commands in external repositories.

Do not use destructive commands on external files.

## Context-loading order

For a selected project, read only what is necessary, preferring:

1. `AGENTS.md`
2. `.ai/STATE.md`
3. `.ai/INDEX.md`
4. Relevant compact project reports
5. Specific source files needed for the task

For a selected agent, prefer:

1. `AGENTS.md`
2. `.ai/STATE.md`
3. `.ai/INDEX.md`
4. `README.md`
5. Relevant architecture or quick-start sections

For a selected verification case, prefer:

1. `verification.yaml`
2. `behavior-catalog.yaml`
3. `invariants.yaml`
4. Redacted `.runtime/` result summaries needed for the task

Never read a case's `.env`, raw `inputs/`, or database exports unless the user
explicitly authorizes the exact artifact and it is required.

For a selected research topic, prefer:

1. `SCOPE.md`
2. `STATE.md`
3. `CONTEXT.md`
4. `GAPS.md`
5. Relevant `REPORT.md` or `SOURCES.md` sections
6. Raw evidence only for a specific unresolved question

For knowledge, prefer:

1. Relevant index
2. Canonical technology, concept, standard, or case-study note
3. Linked evidence only when necessary

Do not load entire repositories, reports, raw crawl directories, session
archives, or datasets into context.

## Evidence rules

Distinguish clearly between:

* Verified reusable knowledge
* Official documented claims
* Project-specific observations
* Experimentally verified results
* Inferences
* Unresolved questions

Do not treat another project's behavior as automatically applicable.

Do not treat documented compatibility as experimental verification.
Do not treat offline snapshot parity as complete live verification.

## Writable output

All generated or modified files must remain inside the current temporary
folder.

Create files only when useful, normally:

* `TASK.md` — normalized objective, scope, and completion criteria
* `SOURCES.md` — selected sources and resolved paths
* `RESULT.md` — final result
* `SESSION_LOG.md` — important actions and consulted files
* `scratch/` — temporary analysis
* `evidence/` — small relevant extracts
* `artifacts/` — generated deliverables

Do not copy complete external reports or repositories into the temporary
folder.

## Task execution

1. Normalize the user's task.
2. Resolve only the named sources.
3. Record selected sources in `SOURCES.md`.
4. Gather the minimum context needed.
5. Perform the requested analysis or creation.
6. Save the complete usable result in `RESULT.md` or `artifacts/`.
7. Record important consulted files and limitations in `SESSION_LOG.md`.
8. Do not update external handoff files.
9. Do not promote findings into knowledge automatically.
10. Do not commit automatically.

## Completion check

Before finishing:

1. Confirm every changed file is inside the current temporary folder.
2. Report which external sources were consulted.
3. Record limitations and unresolved questions.
4. List generated files.
5. Confirm that no external repository was modified.
