# Git Maintenance

Git belongs to the repository that owns the work. The workspace root is not a
repository.

## Start

For a repository-owned target:

1. Resolve the repository with `git -C <target> rev-parse --show-toplevel`.
2. Inspect `git -C <repo> status --short --branch`.
3. Note unrelated modifications and avoid overlapping them.
4. Inspect recent history only when it helps identify conventions or current
   work.

An absent repository is a fact to report, not permission to initialize one.
Initialize Git only when the user requests it or a workspace creation script
defines it as part of creating a new target.

## During work

- Use repository-relative paths in status and diff reviews.
- Do not stage unrelated files.
- Check ignore behavior before generating secrets, database files, raw inputs,
  or large runtime artifacts.
- Keep generated evidence ignored unless its compact, redacted form is an
  intentional deliverable.
- Use worktrees for concurrent write-capable work on one repository.

## Checkpoint

Before recommending or creating a commit:

1. Review `git status --short`.
2. Review `git diff --stat` and the relevant diff.
3. Review staged changes separately.
4. Confirm no credentials, `.env`, dumps, raw records, or unrelated files are
   included.
5. Run relevant tests or clearly report what remains unverified.
6. Suggest one purpose-focused commit message.

Committing and pushing require explicit user authorization. Never silently
amend, force-push, reset, clean, discard, delete branches, or remove worktrees.

## Handoff

Report:

- repository and branch;
- clean or dirty state;
- staged, unstaged, and untracked scope;
- checks actually run;
- whether a commit exists;
- suggested next Git action and commit message.

For coordinated multi-repository work, report these separately for each
repository.
