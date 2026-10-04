# AI Workspace

A folder layout, contract, and small set of scripts that let any AI coding
CLI — Claude Code, Kiro, Codex, Gemini CLI — work across many projects and
research topics from one place, while loading only the context a task needs.

It is not an application. It is a control plane: the root routes work to one
owning target (a project, a research topic, an agent case), and every command,
edit, test, and Git operation is scoped to that target.

## Why

Long AI sessions get expensive because every turn re-reads the whole context.
This workspace keeps the always-loaded part small (a ~2 KB root contract) and
moves everything else behind indexes, state files, and on-demand skills, so a
session loads a few thousand tokens of routing context instead of a whole
repository. See [docs/MEASUREMENTS.md](docs/MEASUREMENTS.md) for the numbers
and how to reproduce them.

## Layout

| Path | Purpose |
|---|---|
| `AGENTS.md` | Root contract read by Codex, Kiro, Gemini (and Claude via `CLAUDE.md` → `@AGENTS.md`) |
| `projects/` | Your application repositories (git-ignored here) |
| `research/topics/` | Source-grounded research packs (git-ignored here) |
| `knowledge/` | Durable verified notes (git-ignored here) |
| `agents/` | Reusable agents; `repo-auditor` and `pipeline-triage` included |
| `_shared/` | Skills, templates, prompts shared by every CLI |
| `tools/local-research/` | Local web research stack (SearXNG, Crawl4AI, Trafilatura, Playwright) |
| `bin/` | Helper scripts |
| `.kiro/` | Kiro custom agents and skill links |

## Quick start

```bash
git clone <this-repo> ~/Workspace && cd ~/Workspace
bin/link-skills            # link _shared/skills into Claude Code, Codex, Kiro
bin/workspace-doctor       # check tools and structure
bin/new-project my-app     # or: bin/new-research my-topic https://docs.example.com
```

Then open your AI CLI at `~/Workspace` and ask it to work on a target, for
example "activate projects/my-app and continue".

Optional: `cd tools/local-research && ./bin/setup && ./bin/start` for local,
credit-free web research.

## Commands

| Command | What it does |
|---|---|
| `bin/new-project <name>` | Create a project with `.ai/STATE.md`, `TASKS.md`, `INDEX.md`, `DECISIONS.md` |
| `bin/new-research <topic> [url]` | Create a research pack with scope, coverage, sources, report, gaps |
| `bin/new-agent-case <agent> <case>` | Create a case for a template-driven agent |
| `bin/workspace-context <target>` | Print a target's instructions, startup files and Git owner, without contents |
| `bin/token-usage` | Local token/cost summary across Claude Code, Codex, Gemini, Kiro (via tokscale; read-only) |
| `bin/session-report` | Does context per turn grow with session length? (local logs, read-only) |
| `bin/selftest` | End-to-end check on a throwaway copy (15 checks) |
| `bin/link-skills` | Link shared skills into each CLI |
| `bin/workspace-doctor` | Health check |
| `bin/export-oss` | Publish your own sanitized copy of the framework (see below) |

## Publishing your own fork

Keep personal work in the git-ignored folders. Mark anything private inside a
framework file with `<!-- private:begin -->` / `<!-- private:end -->`
(Markdown) or `# private:begin` / `# private:end` (shell). `bin/export-oss`
copies only the paths in `_shared/oss/manifest.txt`, strips private blocks,
and refuses to finish if it finds secrets, your home path, or any word in your
local `.oss-denylist`.

## Compatibility

| CLI | Reads | Skills |
|---|---|---|
| Claude Code | `CLAUDE.md` (imports `AGENTS.md`) | `~/.claude/skills` |
| Codex | `AGENTS.md` | `~/.agents/skills` |
| Kiro | `AGENTS.md`, `.kiro/agents`, `.kiro/skills` | `.kiro/skills` |
| Gemini CLI | `AGENTS.md` when configured as its context file | — |

## License

Apache-2.0 — see [LICENSE](LICENSE).

Third-party tools are installed separately and keep their own
licenses; see [docs/THIRD-PARTY.md](docs/THIRD-PARTY.md).
