# Research and Local Web Tools

Default routing for current external evidence:

1. SearXNG for broad local discovery.
2. Source policy for canonicalization, deduplication, and authority ranking.
3. Crawl4AI for clean Markdown extraction.
4. Trafilatura for fast local main-text extraction when static HTTP is sufficient.
5. Playwright for JavaScript-heavy fallback retrieval.
5. Firecrawl only when explicitly selected or an authorized local failure
   fallback.

Tool root: `~/Workspace/tools/local-research/`

Shared skill: `$local-web-research` in Codex and `/local-web-research` in Claude.

The stack removes per-request crawl credits, not physical or policy limits.
Local compute, network, rate limits, robots rules, terms, authentication, and
captchas still constrain retrieval.
