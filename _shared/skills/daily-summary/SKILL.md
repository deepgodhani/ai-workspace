---
name: daily-summary
description: Create or update today's cross-workspace daily summary using reusable-agent states and verification cases, Git history, project states, research states, and session handoffs. Use when the user says daily summary, summarize today, end my day, or what did I do today.
---

# Create today's daily summary

Work from the workspace root.

Use the user's local date from `date +%F`.

Write the summary to:

`knowledge/daily/YYYY/MM/YYYY-MM-DD.md`

Create the directories when necessary.

## Evidence to inspect

Inspect only relevant repositories under:

* `agents/`
* `projects/`
* `research/`
* `knowledge/`

Use:

* Git commits created today
* Git status
* Current branches
* `.ai/STATE.md`
* `.ai/TASKS.md`
* Relevant dated files under `.ai/sessions/`
* Research `STATE.md`
* Research `CONTEXT.md`
* Research coverage changes
* Agent `.ai/STATE.md` and `.ai/TASKS.md`
* The selected case's redacted `.runtime/` stage summaries
* `tools/database-port-registry.yaml` only when allocation changed today

Do not scan:

* `node_modules/`
* `.git/` object storage
* Research `raw/` directories
* Large downloaded datasets
* Environment files
* Agent case `inputs/`
* Raw database exports or records
* Credentials

## Required sections

Create or update:

* Main outcomes
* Workstreams
* Completed work
* Important findings
* Decisions
* Blockers
* Exact next actions
* Git checkpoints
* Knowledge promoted
* Open questions
* Tomorrow

## Rules

* Base the summary only on evidence.
* Keep `PASS`, `FAIL`, `BLOCKED`, and `UNVERIFIED` distinct for agent stages.
* Do not describe offline snapshot fidelity as complete live verification.
* Do not claim commands, tests, crawls, or migrations ran unless recorded.
* Keep the summary concise and useful.
* Link to detailed agent, project, or research files instead of copying them.
* Never include secrets, environment-variable values, or connection strings.
* Do not modify agent, project, or research state files.
* Do not commit automatically.
* If today's file already exists, merge new work into it instead of replacing valid earlier entries.
