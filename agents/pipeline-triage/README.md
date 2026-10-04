# pipeline-triage

Generic triage for any failing scheduled/async job: timeline, root-cause
hypothesis with confidence, blast radius, proposed fix. Diagnosis only — it
never applies the fix. See `AGENTS.md` for the procedure and case pattern.

```bash
cd ~/Workspace
bin/new-agent-case pipeline-triage <case-name>
```
