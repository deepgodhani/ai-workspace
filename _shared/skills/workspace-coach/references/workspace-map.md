# Workspace Map

## Root

`~/Workspace`

The root is an organizer, not a Git repository.

## Development projects

`~/Workspace/projects/<project>/`

Owned context:

* `AGENTS.md`
* `CLAUDE.md`
* `.ai/STATE.md`
* `.ai/TASKS.md`
* `.ai/INDEX.md`
* `.ai/DECISIONS.md`
* `.ai/sessions/`
* `docs/`

## Reusable agents

`~/Workspace/agents/<agent>/`

Reusable agents own code, adapters, tests, documentation, and per-run case
bundles. They are not recreated for each project.

Reusable-agent cases live at:

`~/Workspace/agents/<agent>/cases/<case-name>/`


Local database port ownership lives at:

`~/Workspace/tools/database-port-registry.yaml`

## Research

`~/Workspace/research/topics/<topic>/`

Owned context:

* `SCOPE.md`
* `STATE.md`
* `CONTEXT.md`
* `SOURCES.md`
* `REPORT.md`
* `GAPS.md`
* `COVERAGE.json`
* `raw/`

The complete `research/` directory is one Git repository unless explicitly
changed.

## Knowledge

`~/Workspace/knowledge/`

Categories:

* `technologies/`
* `concepts/`
* `standards/`
* `case-studies/`
* `indexes/`
* `inbox/`
* `daily/`
* `_meta/`

The knowledge directory is its own Git repository.

## Shared resources

`~/Workspace/_shared/`

Categories:

* `prompts/`
* `skills/`
* `templates/`

## Parallel work

`~/Workspace/worktrees/`

Use separate worktrees when two write-capable agents work on the same
project concurrently.

## Information flow

Internet or official documentation
→ research evidence
→ compact research context
→ verified reusable knowledge
→ project-specific decision or implementation
→ experimental evidence
→ updated knowledge

Project artifacts + assessment output + migration snapshots/endpoints
→ reusable verifier case
→ redacted verification evidence
→ project remediation decision
