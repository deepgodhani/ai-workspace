# Cross-Context Rules

Use this retrieval order:

1. Current state and index.
2. For agent work, the selected case manifest and relevant redacted stage
   summary.
3. Relevant knowledge index and one or two canonical notes.
4. Relevant research `CONTEXT.md`.
5. Specific report sections.
6. Raw evidence only for a concrete unresolved question.

External repositories are read-only unless the user explicitly authorizes a
coordinated multi-repository change. Link to existing reports instead of
copying them. Record which external files materially influenced a conclusion.

External agents and cases are also read-only. Select one case, never load
every case, and do not read `.env`, raw `inputs/`, exports, or database records
unless the user explicitly authorizes the exact artifact.
