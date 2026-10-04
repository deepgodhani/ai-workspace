# Third-party software

This repository contains only its own code and documents. Everything below is
**not included**: each tool is installed on your machine, by you or by a setup
script, and stays under its own license. This repository's license applies
only to the files in this repository.

| Software | License | How it is used | Obtained by |
|---|---|---|---|
| SearXNG | AGPL-3.0 | Local search service for `tools/local-research`, run unmodified as a separate process | `tools/local-research/bin/setup` clones it into `tools/local-research/work/` (git-ignored), or the `searxng/searxng` Docker image |
| Crawl4AI | Apache-2.0 | Page crawling (optional `crawler` extra) | `pip`/`uv` install |
| Playwright | MIT (package metadata) | Browser fallback for crawling; downloads its own browser builds | `pip`/`uv` install, `playwright install` |
| Trafilatura | Apache-2.0 | Article text extraction | `pip`/`uv` install |
| httpx | BSD-3-Clause | HTTP client | `pip`/`uv` install |
| beautifulsoup4, markdownify, PyYAML | MIT | HTML parsing, Markdown conversion, YAML | `pip`/`uv` install |
| tokscale | MIT | Read-only token usage from local CLI logs (`bin/token-usage`, `bin/session-report`) | `npx` at run time; never submits data |
| Claude Code, Kiro, Codex CLI, Gemini CLI | Their vendors' terms | The AI CLIs this workspace is designed for | Installed by you from each vendor |
| Firecrawl (optional) | Hosted service | Optional fallback for web research via `bin/firecrawl-mcp-wrapper` | Your own account and API key |

## Rules for contributors

- **Use, don't vendor.** Call tools as separate programs or declare them as
  dependencies. Don't copy their source into this repository.
- **No copyleft code in this repository.** GPL/AGPL projects may be installed
  and run alongside the workspace (as SearXNG is), but their code must not be
  copied in, and a modified version must not be distributed from here.
- **Keep this table current** when adding a dependency: name, license, how
  it's used, how it's obtained.

This file explains how the repository uses third-party software. It is not
legal advice.
