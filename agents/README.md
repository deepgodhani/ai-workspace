# Reusable agents

This directory owns reusable operational systems. It does not contain one
agent per project or per database.

Two kinds of agent live here:

- **Coded** — has its own CLI/app for creating and running cases.
- **Template-driven** (e.g. `repo-auditor`, `pipeline-triage`) — generic,
  tool-agnostic, prompt-and-file based. Create a case with:

  ```bash
  cd ~/Workspace
  bin/new-agent-case <agent-name> <case-name>
  ```

## Generic, domain-agnostic agents

- `repo-auditor/` — read-only audit of any Git repo: dependency/security
  posture, doc-vs-code drift, dead-code signals, test-coverage gaps.
- `pipeline-triage/` — triage of any failing scheduled/async job on any
  platform: timeline, root-cause hypothesis, blast radius, proposed fix.
  Diagnosis only, never applies the fix.

Each has its own `AGENTS.md` with the full procedure and case pattern. These
two are deliberately not tied to any project, client, or stack — the target
is always supplied by the case, never baked into the agent.

