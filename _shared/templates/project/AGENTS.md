# {{PROJECT_NAME}} Agent Contract

## Session startup

Before changing files:

1. Read `.ai/STATE.md`.
2. Read `.ai/TASKS.md`.
3. Read `.ai/INDEX.md`.
4. Inspect `git status`.
5. Open additional context only when relevant.

## Working rules

- Work only inside this repository.
- Search before opening large files.
- Preserve existing behavior unless the task explicitly changes it.
- Do not add dependencies without explaining the need.
- Never write secrets into source-controlled files.
- Run relevant tests, linting, formatting, or type checks after changes.
- Record durable architectural choices in `.ai/DECISIONS.md`.
- Put project-specific research under `docs/research/`.

## Context budget

- Keep `.ai/STATE.md` below roughly 1,500 words.
- Do not copy terminal transcripts or large documents into `STATE.md`.
- Use `.ai/INDEX.md` to point to detailed context.
- Do not automatically load `.ai/sessions/` or raw research.

## Session completion

Before ending meaningful work:

1. Update `.ai/STATE.md` with the exact next action.
2. Update `.ai/TASKS.md`.
3. Update `.ai/DECISIONS.md` when necessary.
4. Write a dated detailed note under `.ai/sessions/`.
5. Record which verification commands actually ran.
