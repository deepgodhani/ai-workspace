# AI Research and Development Workspace

This directory is an organizer, not one large software repository.

## Layout

- `projects/` — independent development repositories
- `research/` — source-grounded research packs
- `knowledge/` — durable personal knowledge in Markdown
- `worktrees/` — temporary Git worktrees for parallel agents
- `_shared/` — templates and reusable prompts
- `tools/` — local infrastructure, including the credit-free research stack
- `bin/` — workspace helper commands (wrappers around `ws`; build it with `make -C tools/ws`)

## Normal usage

The workspace root is the normal control plane. `$workspace-coach` may plan and
execute against any folder by activating one target and scoping commands,
edits, tests, and Git operations to it. You do not need to change directories.

Inspect a target without loading its contents:

```bash
./bin/workspace-context projects/my-project
```

Create a project from the root:

```bash
./bin/new-project my-project
```

Create a research topic from the root:

```bash
./bin/new-research kubernetes https://kubernetes.io/docs/
```

Create a portable, CLI-neutral workspace in any folder:

```bash
./bin/init-ai-workspace.sh init /path/to/new-workspace
cd /path/to/new-workspace
./workspace begin
```

Use `./workspace begin` for default activation of the current environment, or
pass a requirement to have Workspace Coach tailor and finalize the environment:

```bash
./workspace begin custom --cli codex --prompt "A workspace for two services and shared research"
```

For each task, select the smallest owning target. Root-driven work must read the
target's applicable `AGENTS.md`, keep one primary write scope, use the target as
the command working directory, and use `git -C <repo>` rather than treating the
workspace root as a repository.

Long conversations still accumulate tokens. After a handoff, start a fresh
conversation at `~/Workspace`; changing folders is unnecessary.

## Session continuity

Every project uses:

- `.ai/STATE.md` — compact current state and exact next action
- `.ai/TASKS.md` — active backlog
- `.ai/INDEX.md` — pointers to detailed context
- `.ai/DECISIONS.md` — durable decisions
- `.ai/sessions/` — detailed session archives

Use `_shared/prompts/start-session.md` at the start of a fresh chat and
`_shared/prompts/end-session.md` before ending it.

Load `STATE.md`, `TASKS.md`, and `INDEX.md` first. Search decisions and open
session notes or raw evidence only when the active task requires them.

## Local web research

```bash
cd ~/Workspace/tools/local-research
./bin/setup
./bin/start
./bin/status
./bin/test --integration
```

From a research topic, invoke `$local-web-research` in Codex or
`/local-web-research` in Claude. SearXNG, Crawl4AI, and Playwright are the
default path; Firecrawl remains an explicitly selected fallback.

## Kiro

Open Kiro at this workspace root to use the same workspace there. `.kiro/skills/`
already symlinks to `_shared/skills/`. `.kiro/agents/` holds custom-agent
configs for the generic reusable agents (currently `repo-auditor` and
`pipeline-triage`) so Kiro's main agent can delegate to them as isolated,
parallel sub-agents — each config points back at its agent's `AGENTS.md`
under `agents/` rather than duplicating the procedure. Default resource
inheritance is disabled in `.kiro/settings/cli.json`
(`chat.disableInheritingDefaultResources`), so every custom agent config
explicitly lists the root `AGENTS.md` plus its own agent's files in
`resources`.
