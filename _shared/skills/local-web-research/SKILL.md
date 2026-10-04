---
name: local-web-research
description: Performs source-grounded web discovery and crawling with the local SearXNG, Crawl4AI, Trafilatura, and Playwright stack, ranks source quality, tracks measurable coverage, and writes research-topic outputs without paid crawl credits.
---

# Local Web + AI Research

Use the self-hosted stack at `~/Workspace/tools/local-research`. Do not require
Firecrawl, hosted Crawl4AI, an LLM API key, or another paid search/crawl API.

This is a hybrid workflow: use the agent's reasoning and existing knowledge to
decompose the question, generate hypotheses, compare evidence, and synthesize the
answer; automatically use live web retrieval to discover newer context and validate
externally checkable claims. The web is the evidence layer. Model memory is not a
citation and must not be presented as one.

## Before retrieval

1. Confirm the current working directory is one research topic under
   `~/Workspace/research/topics/` or an explicitly authorized disposable topic.
2. Read `AGENTS.md`, `SCOPE.md`, `STATE.md`, `CONTEXT.md` when it contains prior
   findings, and `GAPS.md`.
3. Read `COVERAGE.json` when continuing an existing crawl.
4. Confirm the research question, versions, allowed domains, exclusions, and
   the current topic as the only write destination.
5. Run `~/Workspace/tools/local-research/bin/status`. An active research request
   authorizes starting the local-only services when they are stopped; report that
   they were started. Do not start them for a read-only status or review request.

If no research topic owns the work, route it with `$workspace-coach` before
creating durable output. Do not write a research pack at the workspace root.

## Retrieval workflow

1. Normalize the question and identify relevant product names, versions, dates,
   terminology, decisions, and claims that need evidence.
2. Use agent reasoning to create initial hypotheses and search queries. Label these
   as hypotheses until validated; do not cite model memory.
3. Search with local SearXNG automatically. For an existing completed topic, target
   newer material, changed documentation, contradictions, and recorded gaps before
   considering a broad recrawl.
4. Prefer tier A and B sources from the source quality policy.
5. Canonicalize URLs, remove tracking parameters, and deduplicate before crawl.
6. Apply allow/deny policy and record exclusions.
7. Crawl selected pages with Crawl4AI, respecting `robots.txt` and site policy.
8. When Crawl4AI misses, use local Trafilatura HTTP extraction for static pages before launching a second browser. Use Playwright only for JavaScript-heavy fallback retrieval; retain failures.
9. Validate important claims against retrieved page content. When practical, use
   two independent sources for consequential claims and prefer the primary source.
10. Record retrieval timestamps, publication/update dates when available, method,
    status, redirects, source tier, contradictions, version uncertainty,
    inaccessible sources, and missing evidence.
11. Synthesize the evidence with agent reasoning. Clearly distinguish sourced
    facts, supported inferences, model-generated hypotheses, and unresolved claims.
12. Write `SOURCES.md`, `REPORT.md`, `CONTEXT.md`, `GAPS.md`, `COVERAGE.json`,
    and selected raw Markdown only inside the current topic. Important claims in
    reports must have nearby source IDs or links resolvable through `SOURCES.md`.

Example:

```bash
~/Workspace/tools/local-research/bin/research \
  "the scoped research question" \
  --output "$PWD" \
  --max-pages 5
```

For discovery without writes:

```bash
~/Workspace/tools/local-research/bin/search "the scoped search query"
```

For one explicitly approved URL:

```bash
~/Workspace/tools/local-research/bin/crawl "https://example.com/docs/page" \
  --output raw/example-page.md
```

## Evidence rules

- Search snippets are discovery metadata, not evidence.
- Model memory is useful for planning and synthesis, but is not evidence or a
  source. Externally checkable claims must be validated against retrieved content.
- Important conclusions should rely mainly on primary authority or strong
  first-party evidence.
- Current, version-sensitive, disputed, or high-impact claims must receive a live
  web check even when the agent already appears to know the answer.
- Preserve meaningful disagreement between credible sources instead of forcing a
  false consensus.
- A numerical source score assists triage; it never replaces judgment.
- Separate sourced facts, interpretations, and experimental observations.
- Do not claim complete coverage without discovered, in-scope, processed,
  failed, and excluded counts.
- Do not bypass authentication, paywalls, captchas, or access controls.
- Do not automatically commit output.
- Keep full extracted Markdown in `raw/`, but send an LLM only the selected, de-noised excerpts needed for the question.

Read `references/source-quality-policy.md` when ranking or adjudicating sources.

## Managed fallback

Firecrawl is optional, not part of the default path. Use it only when the user
explicitly selects it or when the local path has failed and the user authorizes
the managed fallback. Record that choice and any credit or privacy implication.

## Completion report

Report the topic path, query, coverage counts, top authoritative sources,
failed/excluded URLs, extraction methods used, generated files, validated claims,
material contradictions, and remaining limitations. Include citations or source
IDs for conclusions. Never report a service or crawl as successful without a
command or artifact that verifies it.
