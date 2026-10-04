# Workspace Agent Contract

`~/Workspace` is an organizer and control plane for independent targets, not
one application or Git repository.

## Scope

- Select the smallest owning target; never scan every project by default.
- Keep one primary write target. Other folders are read-only unless the user
  explicitly requests a coordinated change.
- A session may stay at the root, but commands, edits, tests, and Git must be
  scoped to the activated target.
- Read applicable `AGENTS.md` files down to the target before editing.
- Load compact state and indexes first; open decisions, reports, sessions, raw
  data, `.runtime`, or `inputs` only for a specific need.
- Preserve existing files and never put credentials in tracked content.

## Locations

- Reusable workflows and cases: `agents/`
- Application repositories: `projects/`
- Research topics: `research/topics/`
- Durable verified knowledge: `knowledge/`
- Shared skills/templates: `_shared/`
- Local infrastructure: `tools/`
- Parallel worktrees: `worktrees/`

## Root execution and Git

- Use the target as each command's working directory.
- Use `git -C <repo>`; never run Git against the workspace root implicitly.
- Preserve unrelated changes. Inspect status/diff and secret or generated-file
  exposure before handoff or a requested commit.
- Do not commit, push, rewrite history, discard changes, or manage branches or
  worktrees without explicit authorization.
- Use `bin/workspace-context <target>` to resolve instructions, startup paths,
  context size, and owning Git repository without loading file contents.

## Reusable agents

- Reuse the owning agent and create one case per independent verification.
- Keep application changes in their owning project; cases own configuration
  and redacted evidence.
- Reserve endpoints in `tools/database-port-registry.yaml`; reachability does
  not prove ownership or identity.
- Store secrets only in ignored environment files or process environment.

## Maintenance

Create projects, research topics, and verification cases with `bin/` scripts.
Use `$local-web-research` for current external research by default; Firecrawl
is optional.
