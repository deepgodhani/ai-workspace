# Folder Structure

The workspace root is an organizer and control plane, never one repository.

- `agents/<agent>/` owns reusable operational workflows and per-run cases.
- `projects/<name>/` owns application implementation and project-specific facts.
- `research/topics/<topic>/` owns evidence, source analysis, coverage, and gaps.
- `knowledge/` owns verified reusable conclusions.
- `_shared/` owns reusable skills, prompts, and templates.
- `tools/` owns local infrastructure such as `tools/local-research/`.
- `temp-sessions/<name>/` owns disposable cross-context output.
- `worktrees/` owns isolated parallel Git worktrees.

Choose one primary write destination. Read other repositories only when their
compact indexes or context directly support the task.

A conversation may remain rooted at `~/Workspace`. Use the selected target as
the working directory for its commands and use its owning repository for Git.

For agent-case work, the primary write destination is normally
`agents/<agent>/cases/<case-name>/`. A related application under `projects/`
is read-only unless application remediation is a separately authorized task.

