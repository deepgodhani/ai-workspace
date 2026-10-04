# AI Research and Development Workspace — Help Manual

This workspace is designed for using multiple AI CLI tools—such as Codex and Claude Code—without depending on one chat session or one model's private memory.

The main principle is:

> Chats are temporary. Files are memory. Git records changes. Agents are replaceable workers.

---

## 1. Mental model

The workspace has three different kinds of information:

1. **Development context** — what is currently happening inside a software project.
2. **Research evidence** — source pages, crawl coverage, findings, gaps, and reports for a topic.
3. **Durable knowledge** — compact conclusions that remain useful across projects and research sessions.

These must not be mixed together.

- Put implementation state in `projects/<project>/.ai/`.
- Put detailed source-grounded investigations in `research/topics/<topic>/`.
- Put reusable, cleaned knowledge in `knowledge/`.

The workspace root is an organizer. It is not one giant project.

---

## 2. Complete folder structure

```text
workspace-root/
├── HELP.md
├── WORKSPACE.md
├── AGENTS.md
├── CLAUDE.md
│
├── bin/
│   ├── new-project
│   ├── new-research
│   ├── workspace-doctor
│   ├── configure-firecrawl
│   └── firecrawl-mcp-wrapper
│
├── tools/
│   └── local-research/
│       ├── docker-compose.yml
│       ├── bin/
│       ├── config/
│       ├── src/
│       └── tests/
│
├── _shared/
│   ├── prompts/
│   │   ├── start-session.md
│   │   ├── end-session.md
│   │   └── research-from-url.md
│   └── templates/
│       ├── project/
│       └── research/
│
├── projects/
│   └── example-project/
│       ├── AGENTS.md
│       ├── CLAUDE.md
│       ├── README.md
│       ├── .ai/
│       │   ├── STATE.md
│       │   ├── TASKS.md
│       │   ├── INDEX.md
│       │   ├── DECISIONS.md
│       │   └── sessions/
│       ├── docs/
│       │   └── research/
│       └── source-code-and-project-files...
│
├── research/
│   ├── AGENTS.md
│   ├── CLAUDE.md
│   ├── README.md
│   └── topics/
│       └── example-topic/
│           ├── AGENTS.md
│           ├── CLAUDE.md
│           ├── SCOPE.md
│           ├── STATE.md
│           ├── COVERAGE.json
│           ├── SOURCES.md
│           ├── REPORT.md
│           ├── CONTEXT.md
│           ├── GAPS.md
│           └── raw/
│
├── knowledge/
│   ├── README.md
│   ├── inbox/
│   ├── concepts/
│   ├── technologies/
│   ├── companies/
│   ├── standards/
│   └── indexes/
│
└── worktrees/
```

---

## 3. Root files

### `HELP.md`

This manual. Read it when you forget where information belongs or how to begin a task.

### `WORKSPACE.md`

A short overview containing the most common commands. Keep it much shorter than this manual.

### `AGENTS.md`

Rules for an AI agent operating at the workspace level. It tells agents that the root contains multiple independent projects and that they must not scan or modify everything automatically.

Use the root agent only for tasks such as:

- Creating a project or research topic.
- Updating shared templates.
- Maintaining helper scripts.
- Organizing the workspace.

Do not use a root-level agent for normal application development.

### `CLAUDE.md`

Claude Code's entry file. It imports the shared rules from `AGENTS.md`:

```markdown
@AGENTS.md
```

This avoids maintaining separate instruction sets for Codex and Claude.

---

## 4. `bin/` — workspace commands

### `bin/new-project`

Creates a new independent development repository with the standard AI context files.

```bash
./bin/new-project billing-service
cd projects/billing-service
```

### `bin/new-research`

Creates a source-grounded research pack.

With a URL:

```bash
./bin/new-research kubernetes https://kubernetes.io/docs/
```

Without a URL:

```bash
./bin/new-research event-sourcing
```

When no URL is supplied, the research agent must first identify authoritative sources.

### `bin/workspace-doctor`

Checks required CLI tools, workspace folders, and MCP configuration.

```bash
./bin/workspace-doctor
```

Run it after initial setup or when an AI CLI tool stops working.

### `bin/configure-firecrawl`

Optional managed fallback. Stores the Firecrawl credential outside Git and
connects the Firecrawl MCP server to installed AI CLI clients. It is no longer
the default research path.

```bash
./bin/configure-firecrawl
```

Never put the API key inside `AGENTS.md`, `.env` committed to Git, prompts, research notes, or source code.

### `bin/firecrawl-mcp-wrapper`

Internal launcher used by Codex and Claude. Normally, you do not run it manually.

### `tools/local-research/`

The default credit-free research playground. It runs local SearXNG for search,
Crawl4AI for extraction and its interactive playground, and Playwright for
browser fallback.

```bash
cd ~/Workspace/tools/local-research
./bin/setup
./bin/start
./bin/status
./bin/test --integration
```

Use `$local-web-research` in Codex or `/local-web-research` in Claude. The local
stack removes per-request credits but remains subject to local resources,
network conditions, website rate limits, robots rules, terms, and captchas.

---

## 5. `_shared/` — templates and reusable prompts

### `_shared/templates/project/`

The starting structure copied into every new project. Change this template when you want future projects to use new conventions.

Changing the template does not automatically update existing projects.

### `_shared/templates/research/`

The standard files copied into every new research topic.

### `_shared/prompts/start-session.md`

A reusable instruction for starting a fresh AI session with few tokens.

Typical use:

```text
Follow the instructions in the workspace start-session prompt and continue the current task.
```

Or paste its contents into the CLI.

### `_shared/prompts/end-session.md`

Tells the current agent to compress its work into files before you close the session.

Run this before changing models, closing the terminal, or starting a new chat.

### `_shared/prompts/research-from-url.md`

The standard process for mapping, crawling, sourcing, and reporting on documentation from a root URL.

---

## 6. `projects/` — software development

Every project under `projects/` should be a separate Git repository.

Example:

```text
projects/payment-api/
projects/mobile-app/
projects/internal-dashboard/
```

Run Codex or Claude from the specific project directory:

```bash
cd projects/payment-api
codex
```

or:

```bash
cd projects/payment-api
claude
```

Do not normally run the agent from `projects/` or from the workspace root.

### Project `AGENTS.md`

Contains stable project working rules:

- Build and test expectations.
- Important boundaries.
- Code conventions.
- Context loading rules.
- Session handoff requirements.

Keep it stable and concise. Do not use it as a daily progress log.

### Project `README.md`

Human-facing project documentation:

- What the application does.
- Installation and setup.
- Commands.
- Basic architecture.
- How to run or deploy it.

### Project `.ai/`

This is the model-independent working memory for the project.

#### `.ai/STATE.md`

The most important context file.

It answers:

- What are we trying to do now?
- What has already been completed?
- What behavior has been verified?
- What is blocked or uncertain?
- Which files are currently relevant?
- What is the exact next action?

Keep it below approximately 1,500 words. A new session should be able to read this file and continue without reading old chats.

Recommended structure:

```markdown
# Current objective

# Current status

# Constraints

# Files currently involved

# Verification

# Blockers and uncertainties

# Exact next action
```

Do not place these in `STATE.md`:

- Full terminal transcripts.
- Large code samples.
- Entire research reports.
- Historical details that no longer affect the next task.
- Speculation presented as verified fact.

#### `.ai/TASKS.md`

The active project backlog.

Use three sections:

```markdown
# Active

# Next

# Completed
```

Keep `Active` small. Ideally, one primary task and only a few immediate subtasks.

#### `.ai/INDEX.md`

A map to deeper context.

Example:

```markdown
# Context Index

## Architecture

- Authentication: `docs/authentication.md`
- Data model: `docs/data-model.md`

## Research

- OAuth research: `docs/research/oauth/CONTEXT.md`

## Operations

- Deployment: `docs/deployment.md`
```

A new AI session reads the index and opens only relevant files. This is how you reduce token use.

#### `.ai/DECISIONS.md`

A durable architecture decision log.

Record decisions that future agents must understand, such as:

- Why PostgreSQL was selected.
- Why a queue was added.
- Why a public API field cannot be renamed.
- Why one authentication strategy was rejected.

Do not record routine implementation details.

Use this format:

```markdown
### 2026-07-14 — Use database-backed job queue

- **Status:** accepted
- **Context:** We need retryable background processing.
- **Decision:** Use the existing PostgreSQL database for the first version.
- **Consequences:** Fewer services, but lower isolation than a dedicated broker.
```

#### `.ai/sessions/`

Detailed dated handoff archives.

Suggested filenames:

```text
2026-07-14-auth-refresh-token.md
2026-07-15-auth-review.md
```

A session note can contain:

- Commands run.
- Detailed changes.
- Investigation paths.
- Failed approaches.
- Test output summary.
- Useful links.

New sessions should not read all session files automatically. They are an archive used only when `STATE.md` or a problem points to one.

### Project `docs/research/`

Contains compact research that is directly relevant to this project.

Do not copy an entire web crawl into a project. Copy or link only:

- `CONTEXT.md`
- `SOURCES.md`
- Selected detailed notes needed by implementation

---

## 7. `research/` — source-grounded investigation

Research is separate from development because it can be large, exploratory, and uncertain.

Each topic is an independent pack:

```text
research/topics/kubernetes/
research/topics/oauth-security/
research/topics/vector-databases/
```

Run the agent from the topic directory:

```bash
cd research/topics/kubernetes
claude
```

or:

```bash
cd research/topics/kubernetes
codex
```

### `SCOPE.md`

Defines what the research should and should not cover.

Include:

- Topic or root URL.
- Questions to answer.
- Versions.
- Included domains and paths.
- Excluded paths.
- Expected deliverables.

Without a clear scope, an agent can crawl too much, miss important sections, or produce an unfocused report.

### `STATE.md`

The current research progress and exact next action.

It should say whether the agent is currently:

- Discovering official sources.
- Mapping a documentation site.
- Crawling pages.
- Extracting findings.
- Validating contradictions.
- Writing the report.
- Producing compact context.

### `COVERAGE.json`

The measurable crawl and processing ledger.

Example:

```json
{
  "root": "https://docs.example.com",
  "version": "4.2",
  "counts": {
    "discovered": 842,
    "in_scope": 721,
    "processed": 704,
    "failed": 8,
    "excluded": 9
  },
  "urls": []
}
```

A report should not claim complete research merely because many pages were visited. Coverage must be stated using counts and unresolved failures.

### `SOURCES.md`

An annotated source list.

For each important source, record:

- URL.
- Source authority.
- Documentation or software version.
- Retrieval date.
- What information the source supports.
- Whether it is primary or secondary.

### `REPORT.md`

The detailed research output. It may be long.

Typical sections:

- Executive summary.
- Purpose and use cases.
- History and versions.
- Core concepts.
- Architecture.
- Installation and configuration.
- APIs and command-line tools.
- Workflows and examples.
- Security.
- Performance and scaling.
- Limitations.
- Troubleshooting.
- Ecosystem and integrations.
- Alternatives and comparisons.
- Compatibility and migration.
- Coverage statement.

### `CONTEXT.md`

The compact reusable version of the research.

This is what future AI sessions should normally read—not the entire report and not all raw pages.

Keep it approximately below 4,000 tokens and include links to deeper sections.

A useful `CONTEXT.md` answers:

- What is this technology?
- What are its important concepts?
- How is it structured?
- What are its common workflows?
- What constraints and pitfalls matter?
- Which versions matter?
- Where should the agent read for more detail?

### `GAPS.md`

Records uncertainty honestly:

- Inaccessible pages.
- Authentication-protected information.
- Contradictory documentation.
- Failed crawls.
- Missing version information.
- Questions requiring experiments or human confirmation.

### `raw/`

Stores source snapshots, extracted pages, and other archival material.

Raw material is evidence, not context. Never load the whole directory into a model at once.

---

## 8. `knowledge/` — durable reusable understanding

The knowledge repository is your long-term personal knowledge base.

It should contain cleaned, synthesized notes—not raw crawls and not temporary project state.

You can open this folder as an Obsidian vault because the notes are normal Markdown files.

### `knowledge/inbox/`

Temporary notes waiting to be processed.

Examples:

- A quick idea.
- A copied link.
- A rough observation.
- A note captured during development.

Review the inbox periodically and either:

- Move the note to a permanent category.
- Merge it into an existing note.
- Convert it into a research topic.
- Delete it if it has no durable value.

### `knowledge/concepts/`

General ideas that are not tied to one product.

Examples:

```text
knowledge/concepts/eventual-consistency.md
knowledge/concepts/idempotency.md
knowledge/concepts/capability-security.md
```

### `knowledge/technologies/`

Compact notes about tools, languages, frameworks, databases, and platforms.

Examples:

```text
knowledge/technologies/kubernetes.md
knowledge/technologies/postgresql.md
knowledge/technologies/firecrawl.md
```

### `knowledge/companies/`

Durable notes about organizations, vendors, and products where organizational context matters.

### `knowledge/standards/`

Protocols, specifications, regulations, and formal standards.

Examples:

```text
knowledge/standards/oauth-2-1.md
knowledge/standards/http-semantics.md
```

### `knowledge/indexes/`

Maps that help humans and agents discover related notes.

Example:

```markdown
# Authentication Index

- [[OAuth 2.1]]
- [[OpenID Connect]]
- [[Session Security]]
- [[Refresh Token Rotation]]
```

Use ordinary relative Markdown links when possible so tools other than Obsidian can understand them.

---

## 9. How knowledge moves through the workspace

Use this lifecycle:

```text
Internet or question
        ↓
research/topics/<topic>/raw/ and SOURCES.md
        ↓
REPORT.md
        ↓
CONTEXT.md
        ↓
knowledge/<category>/<durable-note>.md
        ↓
projects/<project>/docs/research/<relevant-pack>/
```

### Stage 1: Capture evidence in research

The research pack contains sources, coverage, uncertainty, and detailed findings.

### Stage 2: Compress into `CONTEXT.md`

Remove duplication and retain the facts, concepts, constraints, and source links that future agents need.

### Stage 3: Promote durable knowledge

Move broadly reusable understanding into `knowledge/`.

A knowledge note should not say:

> We are currently fixing refresh tokens in Project A.

That belongs in the project's `.ai/STATE.md`.

A knowledge note can say:

> Refresh-token rotation invalidates the previous token after use and should include reuse detection for stolen token families.

That is durable knowledge.

### Stage 4: Attach relevant knowledge to a project

When a project needs the research, copy or reference its compact context under:

```text
projects/<project>/docs/research/<topic>/
```

Then add the path to `.ai/INDEX.md`.

---

## 10. Daily development workflow

### Start

```bash
cd /path/to/workspace/projects/my-project
git status
codex
```

Give the new session this instruction:

```text
Read AGENTS.md, .ai/STATE.md, .ai/TASKS.md, and .ai/INDEX.md.
Continue from the exact next action in STATE.md. Load additional files only when relevant.
```

### During work

The agent should:

1. Work only in the project repository.
2. Search before loading large files.
3. Update tests with behavior changes.
4. Record durable decisions.
5. Keep temporary terminal output out of context files.

### End

Give the agent:

```text
Prepare a fresh-session handoff using the workspace end-session prompt.
Update STATE.md, TASKS.md, DECISIONS.md where needed, and add a dated session note.
Record only verification commands that actually ran.
```

Then review and commit:

```bash
git diff
git status
git add .
git commit -m "Describe the completed unit of work"
```

The next chat can now continue using files rather than the old conversation.

---

## 11. Daily research workflow

### Create the topic

```bash
cd /path/to/workspace
./bin/new-research redis https://redis.io/docs/latest/
cd research/topics/redis
```

### Review scope

Edit `SCOPE.md` before a large crawl.

Questions to resolve:

- Which documentation version?
- Is API reference included?
- Are tutorials included?
- Are blog posts included?
- Are GitHub repositories included?
- Should old versions be excluded?
- Are non-English duplicates excluded?

### Start the agent

```bash
claude
```

Prompt:

```text
Read AGENTS.md, SCOPE.md, and STATE.md.
Map the root site before crawling broadly.
Update COVERAGE.json, SOURCES.md, REPORT.md, CONTEXT.md, GAPS.md, and STATE.md.
Prefer primary sources and report failed or excluded pages explicitly.
```

For external retrieval, invoke `$local-web-research` in Codex or
`/local-web-research` in Claude. The default pipeline is SearXNG → source
ranking and deduplication → Crawl4AI → Playwright fallback. Firecrawl is used
only when explicitly selected or authorized after local failure.

### End the research session

Require the agent to update `STATE.md` with:

- Current phase.
- Counts completed.
- Important verified findings.
- Problems encountered.
- Exact next query, crawl, validation, or writing action.

This allows another model or a new chat to continue.

---

## 12. How to use multiple AI tools

Codex and Claude should share files, not chat transcripts.

A good pattern is:

- **Codex:** implementation, refactoring, tests, repository-oriented tasks.
- **Claude:** broad investigation, design review, long-document synthesis, alternative analysis.

This is a working preference, not a hard rule. Use whichever performs better for the specific task.

### Sequential use

1. Claude researches or reviews.
2. It writes findings into project or research files.
3. End the Claude session with a proper handoff.
4. Start Codex in a new session.
5. Codex reads the same state and continues implementation.

### Parallel use

Do not run two write-capable agents in the same working tree.

Use Git worktrees:

```bash
cd projects/my-project

git worktree add ../../worktrees/my-project-codex -b ai/codex-task
git worktree add ../../worktrees/my-project-claude -b ai/claude-review
```

Possible division:

- Codex implements a feature.
- Claude independently reviews the design or writes test cases.
- Merge only after reviewing each branch.

---

## 13. Context layers and token control

Use four context levels.

### Level 0 — Permanent instructions

Files:

```text
AGENTS.md
CLAUDE.md
```

Loaded every session. Keep concise.

### Level 1 — Current state

Files:

```text
.ai/STATE.md
.ai/TASKS.md
```

Loaded at the start of most project sessions.

### Level 2 — On-demand context

Files referenced by:

```text
.ai/INDEX.md
research/.../CONTEXT.md
knowledge/indexes/
```

Load only when relevant.

### Level 3 — Archive and evidence

Examples:

```text
.ai/sessions/
research/.../raw/
REPORT.md
git history
old logs
```

Read only when investigating a specific question.

### Token-saving rules

- Do not ask the model to scan the whole workspace.
- Start the CLI in the smallest relevant directory.
- Keep `STATE.md` compact.
- Use indexes instead of giant context files.
- Search filenames and text before opening full documents.
- Store source material once and link to it.
- Summarize stable findings into `CONTEXT.md`.
- Archive completed work rather than keeping it in current state.
- Start a fresh chat after a clean handoff when the current conversation becomes noisy.

---

## 14. Naming conventions

Use lowercase kebab-case for directories and files when practical:

```text
refresh-token-rotation.md
event-sourcing
payment-api
```

Research topics should be specific:

Good:

```text
postgresql-row-level-security
kubernetes-network-policies
openai-api-batch-processing
```

Too broad:

```text
databases
cloud
ai
```

Session filenames should begin with the date:

```text
2026-07-14-initial-architecture.md
```

---

## 15. What belongs where

| Information | Location |
|---|---|
| Current coding objective | `projects/<project>/.ai/STATE.md` |
| Active implementation checklist | `projects/<project>/.ai/TASKS.md` |
| Architecture decision | `projects/<project>/.ai/DECISIONS.md` |
| Detailed previous session | `projects/<project>/.ai/sessions/` |
| Project setup instructions | `projects/<project>/README.md` |
| Detailed web investigation | `research/topics/<topic>/REPORT.md` |
| Research source list | `research/topics/<topic>/SOURCES.md` |
| Crawl status | `research/topics/<topic>/COVERAGE.json` |
| Compact reusable research | `research/topics/<topic>/CONTEXT.md` |
| Raw webpage extraction | `research/topics/<topic>/raw/` |
| General reusable insight | `knowledge/concepts/` |
| Reusable technology note | `knowledge/technologies/` |
| Quick unprocessed note | `knowledge/inbox/` |
| Project-specific research copy | `projects/<project>/docs/research/` |
| Reusable prompt | `_shared/prompts/` |
| New-project defaults | `_shared/templates/project/` |

---

## 16. Common mistakes

### Mistake: Running an agent at the workspace root

Result: it may scan unrelated projects and waste tokens.

Fix: `cd` into the exact project or research topic first.

### Mistake: Using one huge `CONTEXT.md`

Result: every new session becomes expensive and outdated.

Fix: keep a small `STATE.md`, a compact `CONTEXT.md`, and an `INDEX.md` that points to detailed files.

### Mistake: Storing progress only in chat

Result: a new model cannot continue reliably.

Fix: update the handoff files before ending each meaningful session.

### Mistake: Moving raw crawls into the knowledge base

Result: the knowledge repository becomes noisy and difficult to retrieve from.

Fix: keep raw evidence in research and promote only synthesized notes.

### Mistake: Treating AI-generated statements as sources

Result: unsupported claims become permanent knowledge.

Fix: preserve original URLs and distinguish verified facts from interpretation.

### Mistake: Letting two agents edit one checkout

Result: overwritten changes and confusing Git state.

Fix: use separate Git worktrees or run agents sequentially.

### Mistake: Putting secrets into context files

Result: credentials may enter Git or model context.

Fix: keep secrets in protected user configuration or environment variables.

---

## 17. Weekly maintenance

Once a week:

1. Review `knowledge/inbox/`.
2. Remove completed tasks from active project state.
3. Ensure every active project has an exact next action.
4. Convert valuable research `CONTEXT.md` files into durable knowledge notes.
5. Check `GAPS.md` for unresolved important questions.
6. Commit uncommitted knowledge and research changes.
7. Remove stale worktrees after their branches are merged.
8. Run `./bin/workspace-doctor`.

List worktrees:

```bash
git worktree list
```

Remove a finished worktree from its project repository:

```bash
git worktree remove ../../worktrees/my-project-codex
```

---

## 18. Fresh-session checklist

Before starting:

```text
[ ] Am I in the smallest relevant directory?
[ ] Is the Git working tree understood?
[ ] Does STATE.md describe the real current state?
[ ] Is there one exact next action?
[ ] Does INDEX.md point to the required deeper context?
[ ] Are secrets outside tracked files?
```

Before ending:

```text
[ ] STATE.md updated
[ ] TASKS.md updated
[ ] DECISIONS.md updated if needed
[ ] Detailed session note added
[ ] Tests/checks accurately recorded
[ ] Exact next action written
[ ] Changes reviewed with git diff
[ ] Meaningful changes committed
```

---

## 19. First commands after setup

From the workspace root:

```bash
./bin/workspace-doctor
```

Create a trial project:

```bash
./bin/new-project sandbox-app
cd projects/sandbox-app
```

Write the objective in:

```text
.ai/STATE.md
.ai/TASKS.md
```

Start the agent:

```bash
codex
```

Create a trial research topic:

```bash
cd /path/to/workspace
./bin/new-research sample-technology https://example.com/docs/
cd research/topics/sample-technology
claude
```

Delete the trial items later if they are not useful.

---

## 20. One-sentence operating rule

> Begin every session by reading a small state pack, retrieve deeper information only when needed, and end every session by writing a compact handoff for the next fresh model.
