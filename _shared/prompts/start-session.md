Continue the current task using the current workspace context's instructions.

1. Resolve the mode from the working directory: reusable agent, project, or
   research.
2. Read `AGENTS.md` and the compact state/task/index files for that mode.
3. For a reusable agent, identify one selected case and read only its
   `verification.yaml`, behavior catalog, invariants, and needed redacted
   runtime summaries. Never load `.env`, raw inputs, or every case.
4. Retrieve only the additional files relevant to the current task.
5. Check the owning Git working tree when one exists; never treat the workspace
   root as a repository.
6. Continue from the exact next action in the current state file.
7. Verify changes with the owning context's tests or checks.

Do not load old session logs or raw research unless the current state points to them.
