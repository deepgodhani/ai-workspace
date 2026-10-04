# Root Execution

The user may keep every conversation rooted at `~/Workspace`. Root execution
means centralized orchestration, not a shared repository or unrestricted write
access.

## Activation

Resolve one smallest owning target and construct an internal activation
envelope:

- target and type;
- primary write scope;
- explicit external read scopes;
- applicable instruction files;
- required and conditional context;
- owning Git repository, if any;
- acceptance criteria.

Read hierarchical instructions explicitly because nested `AGENTS.md` files are
not guaranteed to be injected when the conversation starts at the workspace
root.

## Commands and edits

- Set the command working directory to the target repository or tool.
- Use paths rooted at `~/Workspace` for patches when necessary.
- Use `git -C <repository>` rather than running Git against the workspace root.
- Run package managers, tests, formatters, and build tools from their owner.
- Treat every non-target project, agent, case, and research topic as read-only.
- For an explicitly coordinated multi-repository task, declare every write
  target and keep separate Git status, verification, and handoff results.

## Switching work

When a request changes the target:

1. stop using the prior write scope;
2. resolve the new owner;
3. read its applicable instructions and compact state;
4. inspect its Git state;
5. continue without asking the user to change directories.

Switching targets in one long chat is supported, but it does not erase prior
conversation tokens. For unrelated or context-heavy work, start a fresh chat
at `~/Workspace` and resume from files.

## Root workspace changes

Files directly under `~/Workspace`, `_shared/`, `bin/`, and workspace-wide
registries are workspace-maintenance targets. They may be changed together
when they form one coherent workspace-maintenance task. They must not be
treated as one application repository.
