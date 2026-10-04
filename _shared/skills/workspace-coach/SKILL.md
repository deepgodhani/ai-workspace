---
name: workspace-coach
description: Operates the ~/Workspace root as a low-token control plane for routing, scoped execution, context retrieval, Git hygiene, reusable agents, research, and knowledge.
---

# Workspace Coach

Operate from `~/Workspace` without treating it as one repository. The user may
plan and execute work from the root; commands, edits, tests, and Git operations
must still be scoped to one owning target.

## Core model

- Root is the control plane.
- One project, agent/case, research topic, knowledge area, tool, or shared
  resource is the primary write target for a task.
- Other locations are read-only unless the user explicitly authorizes a
  coordinated multi-target change.
- Chats are temporary; compact state and Git are persistent memory.
- Indexes route to detail. Raw evidence is never default context.

## Activate a target

Before substantive work:

1. Classify the request.
2. Resolve the smallest owning target.
3. Read the root contract and every applicable `AGENTS.md` down to that target.
4. Load only the target's compact startup context.
5. Identify external read-only context.
6. Inspect repository Git state when the target belongs to a repository.
7. Continue from the root using target-scoped paths and command working
   directories. Do not require the user to change folders.

Keep this activation envelope internal unless the user asks for it:

```yaml
target: relative/path
type: project|agent-case|research|knowledge|tool|shared|temporary
write_scope: [relative/path]
read_scope: [specific paths]
required_context: [specific compact files]
conditional_context: [specific files or sections]
acceptance: [observable outcomes]
git_root: relative/path|null
```

Use `bin/workspace-context <target>` when target resolution or startup context
is unclear. It prints paths and Git metadata, not file contents.

## Minimal context loading

Load paths, then conclusions, then detail:

1. Applicable `AGENTS.md`.
2. `STATE.md`, `TASKS.md`, and `INDEX.md` when present.
3. Selected case `STATUS.md` and manifest for agent-case work.
4. One relevant knowledge note or research `CONTEXT.md`.
5. Specific decision or report sections found with search.
6. Raw logs, JSON, exports, datasets, `raw/`, `inputs/`, or `.runtime/` only
   for a concrete unresolved question.

Do not automatically read `DECISIONS.md`, session archives, full reports,
every case, or every project. Search before opening large files. Do not copy
source material into state files or prompts.

## Task routing

- Project implementation or project-specific analysis → `projects/<name>/`
- Repeatable verification → an existing `agents/<agent>/cases/<case>/`
- Agent framework changes → `agents/<agent>/`
- Current external investigation → `research/topics/<topic>/`
- Verified reusable conclusion → `knowledge/`
- Unverified candidate → `knowledge/inbox/`
- Reusable workflow → `_shared/`
- Local infrastructure → `tools/`
- Disposable cross-context output → `temp-sessions/<name>/`
- Parallel work on one repository → `worktrees/`

Create new projects, topics, and agent cases with the matching script in
`bin/` (`new-project`, `new-research`, `new-agent-case`). Reuse an existing agent for a new run; create a case, not another
agent. Reserve database ports in `tools/database-port-registry.yaml`.

## Root-scoped execution

- Pass the target as the working directory for shell commands.
- Use `git -C <repo>` for Git.
- Patch only paths inside the activated write scope.
- Run tests from the owning repository.
- Never use the workspace root as an implicit build, test, or Git target.
- When the user switches targets, close the previous write scope and activate
  the new one before acting.

A fresh chat may still be useful to discard accumulated conversation, but it
can also start at `~/Workspace`; changing directories is not required.

## Git maintenance

For repository-owned work:

1. Inspect branch/status before changes.
2. Preserve unrelated user changes.
3. Review diff and untracked files before handoff or commit.
4. Keep secrets, `.env`, raw inputs, dumps, and generated runtime data out of
   tracked changes.
5. Run relevant verification before recommending a checkpoint.
6. Keep one purpose per commit.

Do not commit, push, rewrite history, discard changes, delete branches, or
create/remove worktrees unless the user explicitly authorizes that action.
Report dirty state and the suggested commit message at handoff.

## Completion

Update only the owning target:

- `STATE.md`: current status, blocker, exact next action.
- `TASKS.md`: active and immediately next work.
- `DECISIONS.md`: only durable decisions.
- A dated session note: detailed evidence when useful.

Report outcome, checks actually run, changed files, Git state, remaining
blocker, and exact next action. Never claim an unrun check passed.

## Conditional references

Read only the reference needed for the current task:

- Ownership and layout: `references/folder-structure.md`
- Root execution details: `references/root-execution.md`
- Context budgets and compaction: `references/context-budget.md`
- Git maintenance: `references/git-maintenance.md`
- Session and handoff lifecycle: `references/session-workflows.md`
- Cross-context retrieval: `references/cross-context-rules.md`
- Web research: `references/research-local-web-tools.md`
- Knowledge promotion: `references/knowledge-lifecycle.md`
- Disposable analysis: `references/temporary-sessions.md`
- Recovery: `references/troubleshooting.md`

## Safety

Never expose credentials or `.env` values. Never scan the whole workspace
when an index or target is available. Never present documentation as runtime
verification, inference as fact, or another project's behavior as universal.
Preserve valid work and prefer small recoverable changes.
