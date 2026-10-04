# ws — AI Workspace command-line tool

One C++20 binary for workspace features. Build once:

```bash
make            # downloads nlohmann/json 3.11.3 (SHA-256 pinned) into build/, links system SQLite
make test       # end-to-end tests with fake claude/codex/kiro/srt (needs Node only for the test runner)
```

`bin/ws` and `bin/orch` in the workspace root call `build/ws`. Every other `bin/`
tool listed below is a 3-line wrapper that runs `ws <command>` with the workspace
set to its own parent folder; output and exit codes are those of the original
scripts (deliberate differences: `.ai/DECISIONS.md`).

## Commands

| `ws` command | `bin/` wrapper | What it does |
|---|---|---|
| `ws context <target>` | `workspace-context` | Target type, Git state, AGENTS.md chain, startup-context sizes (no file contents) |
| `ws doctor` | `workspace-doctor` | Read-only health check: tools, folders, agents, port registry, skills, local research, MCP servers |
| `ws export-oss [dest] [--check]` | `export-oss` | Stage the manifest paths, strip private blocks, scan for secrets/home path/denylist/emails/escaping links, sync to `dest` only if clean; never commits |
| `ws firecrawl configure` | `configure-firecrawl` | Save the optional Firecrawl key (mode 600) and add its MCP server to Codex and Claude Code |
| `ws firecrawl mcp` | `firecrawl-mcp-wrapper` | Start the Firecrawl MCP server with the saved key (what the CLIs launch) |
| `ws link-skills` | `link-skills` | Link `_shared/skills` into `~/.claude/skills`, `~/.agents/skills`, `.kiro/skills`; never replaces entries |
| `ws new project\|research\|agent-case …` | `new-project`, `new-research`, `new-agent-case` | Scaffold from `_shared/templates/…` or `agents/<agent>/cases/_template` |
| `ws orch …` | `orch` | Scoped Claude Code / Codex / Kiro child sessions: spawn (read-only, or `--write` in a Git worktree), resume, merge, list/show/diff/close; sandboxed by default (`ws orch --help`) |
| `ws selftest` | `selftest` | End-to-end check on a throwaway copy with an isolated HOME (18 checks; exit code = failures) |
| `ws session-report` | `session-report` | Does context per message grow with session length? (local logs via tokscale, read-only) |
| `ws usage` | `token-usage` | Token and cost summary across Claude Code, Codex, Gemini, Kiro (local logs via tokscale, read-only) |
| `ws version` | – | |

`ws <command> --help` prints the options. Not ported on purpose: the installers
(`bin/init-ai-workspace.sh` runs before `ws` is built).

## Private content

`export-oss` strips private blocks only in text files (`.md`, `.sh`, `.py`, … and
files without a suffix), and `tools/ws` is exported whole. Never put private names
in `src/` or `test/`: pass them from the `bin/` wrapper inside a `# private:begin` /
`# private:end` block, as `bin/workspace-doctor` does with `--agent-readme` /
`--check-exec`.

## Requirements

C++20 compiler (clang or gcc), POSIX, SQLite 3 (macOS built-in; `libsqlite3-dev` on
Debian/Ubuntu), `curl` for the first build, `git`. Optional at run time: `npx` for
the srt sandbox tier, `tokscale` reports, and the Firecrawl MCP server. The `bin/`
tools no longer need Python or rsync.
