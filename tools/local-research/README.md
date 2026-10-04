# Local Research Playground

A self-hosted, credit-free web research stack for `~/Workspace`:

- **SearXNG** discovers URLs through a local metasearch service.
- **Crawl4AI** extracts clean Markdown and exposes a local interactive playground.
- **Trafilatura** performs fast, local main-text extraction for static HTML after a Crawl4AI miss.
- **Playwright** is the final browser fallback for JavaScript-heavy pages.
- The CLI canonicalizes and deduplicates URLs, applies domain policy, ranks source authority, records retrieval metadata and failures, and writes research-compatible outputs.

No hosted LLM or paid crawling API is used. “Free” means no per-request credits; local CPU, memory, disk, bandwidth, website policies, rate limits, captchas, and search-engine blocking still apply.

## Prerequisites

- `uv` and Git
- Optional for the web playground UI: Docker Compose, or fully open-source
  Podman + Podman Compose
- About 4 GB of free memory when running both containers

On macOS, an open-source container runtime can be installed with:

```bash
brew install podman podman-compose
podman machine init
podman machine start
```

## Setup and services

```bash
cd ~/Workspace/tools/local-research
./bin/setup
./bin/start
./bin/status
```

`./bin/start` first tries container mode. If containers or virtualization are
unavailable, it automatically starts native SearXNG; crawling continues through
the locally installed Crawl4AI and Playwright Python libraries. Modes can be
selected explicitly:

```bash
./bin/start --native
./bin/start --containers
```

The local-only interfaces are:

- SearXNG: <http://127.0.0.1:8888>
- Crawl4AI playground in container mode: <http://127.0.0.1:11235/playground>
- Crawl4AI monitor in container mode: <http://127.0.0.1:11235/monitor>

Native mode still provides the complete CLI research pipeline without a web UI. The fallback sequence is Crawl4AI → Trafilatura over HTTP → Playwright, so a failed browser launch does not waste another browser attempt before trying low-cost static extraction.

Stop the containers without removing cached data:

```bash
./bin/stop
```

## Commands

Search and rank sources:

```bash
./bin/search "Firestore Enterprise MongoDB compatibility official documentation"
```

Crawl one page to Markdown:

```bash
./bin/crawl "https://cloud.google.com/firestore/docs/enterprise/behavior-differences" --output work/page.md
```

Run a complete research retrieval into an existing or disposable topic directory:

```bash
./bin/research \
  "Firestore Enterprise MongoDB compatibility official documentation" \
  --output /absolute/path/to/topic \
  --max-pages 5
```

The run writes `SOURCES.md`, `REPORT.md`, `CONTEXT.md`, `GAPS.md`, `COVERAGE.json`, and `raw/`. It never commits files.

Unit test:

```bash
./bin/test
```

End-to-end test (requires running SearXNG and network access):

```bash
./bin/test --integration
```

Deterministic pipeline test (SearXNG-compatible fixture plus a real official
page crawl; clearly marked as a fixture, not a live search-engine test):

```bash
./bin/test --fixture-integration
```

## Policy and safety

- `config/domains.yml` controls allow/deny rules and known source tiers.
- `config/source-policy.yml` controls ranking weights and promotional penalties.
- `robots.txt` checks are enabled. Site terms and applicable law remain the operator's responsibility.
- Services bind to `127.0.0.1` only.
- Search snippets are discovery metadata, not evidence.
- Important conclusions should rely mainly on tier A and B sources.
- Firecrawl is not required. It remains outside this tool as a manually selected managed fallback.

## Troubleshooting

- If SearXNG returns HTTP 403 for JSON, confirm `json` remains in `search.formats` in `config/searxng/settings.yml`.
- If the browser is missing, rerun `./bin/setup`.
- If port 8888 or 11235 is busy, change `SEARXNG_PORT` or `CRAWL4AI_PORT` in `.env`, then set the matching `SEARXNG_URL`/`CRAWL4AI_URL` for CLI use.
- Search engines can throttle a private SearXNG instance. Reduce frequency and enable or disable engines in the local SearXNG settings as needed.
- Crawl failures are retained in `GAPS.md` and `COVERAGE.json`; they are not silently discarded.
- For token-efficient synthesis, pass only selected source excerpts to an LLM; retain full extracted Markdown locally as evidence.
