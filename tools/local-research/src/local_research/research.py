from __future__ import annotations

import asyncio
import re
from pathlib import Path

from .core import CrawlResult, SearchResult, safe_stem, utc_now, write_json
from .crawl import crawl_url
from .policy import SourcePolicy
from .search import search_searxng


def _one_line(value: str, limit: int = 500) -> str:
    return re.sub(r"\s+", " ", value).strip()[:limit]


def _write_pack(
    output: Path,
    query: str,
    ranked: list[SearchResult],
    selected: list[SearchResult],
    crawls: list[CrawlResult],
    search_meta: dict,
    excluded: list[dict[str, str]],
    canonical_unique_count: int,
) -> None:
    generated = utc_now()
    output.mkdir(parents=True, exist_ok=True)
    raw_dir = output / "raw"
    raw_dir.mkdir(exist_ok=True)
    by_url = {item.url: item for item in crawls}

    for item in crawls:
        if not item.success:
            continue
        stem = safe_stem(item.url)
        header = (
            "---\n"
            f"source_url: {item.url}\n"
            f"final_url: {item.final_url or item.url}\n"
            f"retrieved_at: {item.retrieved_at}\n"
            f"method: {item.method}\n"
            f"status_code: {item.status_code}\n"
            "---\n\n"
        )
        (raw_dir / f"{stem}.md").write_text(header + item.markdown.rstrip() + "\n", encoding="utf-8")
        write_json(raw_dir / f"{stem}.json", item.to_dict(include_markdown=False))

    successful = [item for item in crawls if item.success]
    failed = [item for item in crawls if not item.success]
    coverage = {
        "query": query,
        "generated_at": generated,
        "search": search_meta,
        "counts": {
            "discovered": int(search_meta.get("raw_result_count", len(ranked))),
            "canonical_unique": canonical_unique_count,
            "in_scope": len(ranked),
            "selected": len(selected),
            "processed": len(successful),
            "failed": len(failed),
            "excluded": len(excluded),
        },
        "urls": [
            {
                "url": source.canonical_url,
                "status": "processed" if by_url.get(source.canonical_url, CrawlResult("", False, "", "")).success else "failed",
                "tier": source.tier,
                "score": source.total_score,
                "method": by_url.get(source.canonical_url).method if by_url.get(source.canonical_url) else None,
                "retrieved_at": by_url.get(source.canonical_url).retrieved_at if by_url.get(source.canonical_url) else None,
                "error": by_url.get(source.canonical_url).error if by_url.get(source.canonical_url) else "not selected",
                "warnings": by_url.get(source.canonical_url).warnings if by_url.get(source.canonical_url) else [],
            }
            for source in selected
        ],
        "excluded_urls": excluded,
    }
    write_json(output / "COVERAGE.json", coverage)

    source_lines = [
        "# Sources",
        "",
        f"- Query: `{query}`",
        f"- Generated: {generated}",
        f"- Search endpoint: {search_meta.get('endpoint', 'unknown')}",
        "",
        "## Ranked sources",
        "",
    ]
    for index, source in enumerate(ranked, 1):
        crawl = by_url.get(source.canonical_url)
        status = "processed" if crawl and crawl.success else "selected but failed" if crawl else "not selected"
        source_lines.extend(
            [
                f"### {index}. {source.title or source.canonical_url}",
                "",
                f"- URL: {source.canonical_url}",
                f"- Tier: {source.tier}",
                f"- Score: {source.total_score}",
                f"- Status: {status}",
                f"- Retrieved: {crawl.retrieved_at if crawl else 'not retrieved'}",
                f"- Method: {crawl.method if crawl else 'not retrieved'}",
                f"- Supports: {_one_line(source.snippet) or 'Discovery result; inspect the source before making claims.'}",
                "",
            ]
        )
    if failed:
        source_lines.extend(["## Failed URLs", ""])
        source_lines.extend(f"- {item.url} — {item.error}" for item in failed)
        source_lines.append("")
    (output / "SOURCES.md").write_text("\n".join(source_lines), encoding="utf-8")

    report_lines = [
        "# Automated Research Retrieval Report",
        "",
        f"## Question\n\n{query}",
        "",
        "## Retrieval summary",
        "",
        f"SearXNG discovered {coverage['counts']['discovered']} results; {len(ranked)} canonical URLs remained after normalization and policy filtering. "
        f"The run selected {len(selected)}, processed {len(successful)}, and failed {len(failed)}.",
        "",
        "This is a retrieval report, not a completed expert synthesis. Claims should be checked against the cited primary sources.",
        "",
        "## Extracted source notes",
        "",
    ]
    for source in selected:
        crawl = by_url.get(source.canonical_url)
        if not crawl or not crawl.success:
            continue
        excerpt = _one_line(crawl.markdown, 1800)
        report_lines.extend(
            [
                f"### {source.title or source.canonical_url}",
                "",
                f"- Source: {source.canonical_url}",
                f"- Authority tier: {source.tier}",
                f"- Retrieved: {crawl.retrieved_at}",
                f"- Extraction: {crawl.method}",
                "",
                excerpt,
                "",
            ]
        )
    (output / "REPORT.md").write_text("\n".join(report_lines), encoding="utf-8")

    context_lines = [
        "# Research Context",
        "",
        f"- Question: {query}",
        f"- Generated: {generated}",
        f"- Coverage: {len(successful)}/{len(selected)} selected URLs processed",
        "",
        "## Best available sources",
        "",
    ]
    for source in selected[:5]:
        crawl = by_url.get(source.canonical_url)
        context_lines.append(
            f"- [{source.title or source.canonical_url}]({source.canonical_url}) — tier {source.tier}, score {source.total_score}; "
            f"{_one_line(source.snippet, 260) or ('retrieved via ' + crawl.method if crawl and crawl.success else 'retrieval failed')}"
        )
    context_lines.extend(
        [
            "",
            "## Use constraints",
            "",
            "- Prefer tier A and B evidence for important conclusions.",
            "- Read `REPORT.md` and the relevant file under `raw/` before citing a detailed claim.",
            "- Treat search snippets as discovery metadata, not evidence.",
        ]
    )
    (output / "CONTEXT.md").write_text("\n".join(context_lines) + "\n", encoding="utf-8")

    gap_lines = ["# Gaps", "", f"Generated: {generated}", ""]
    if failed:
        gap_lines.extend(["## Failed retrievals", ""])
        gap_lines.extend(f"- {item.url} — {item.error}" for item in failed)
        gap_lines.append("")
    if excluded:
        gap_lines.extend(["## Policy exclusions", ""])
        gap_lines.extend(f"- {item['url']} — {item['reason']}" for item in excluded)
        gap_lines.append("")
    gap_lines.extend(
        [
            "## Remaining analysis",
            "",
            "- Validate version applicability and contradictions across the retrieved sources.",
            "- Add direct experiments where documentation alone cannot establish runtime behavior.",
            "- Do not claim complete coverage beyond the counts in `COVERAGE.json`.",
        ]
    )
    (output / "GAPS.md").write_text("\n".join(gap_lines) + "\n", encoding="utf-8")


async def run_research(
    query: str,
    output: Path,
    config_dir: Path,
    searxng_url: str = "http://127.0.0.1:8888",
    max_results: int = 20,
    max_pages: int = 5,
    delay_seconds: float = 1.0,
    retries: int = 1,
    version: str | None = None,
) -> dict:
    policy = SourcePolicy(config_dir)
    search_results, search_meta = await search_searxng(query, searxng_url, limit=max_results)
    ranked = policy.rank(search_results, query, version=version)
    in_scope: list[SearchResult] = []
    excluded: list[dict[str, str]] = []
    for source in ranked:
        allowed, reason = policy.allowed(source.canonical_url)
        if allowed:
            in_scope.append(source)
        else:
            excluded.append({"url": source.canonical_url, "reason": reason})
    selected = in_scope[:max_pages]
    crawls: list[CrawlResult] = []
    for index, source in enumerate(selected):
        if index:
            await asyncio.sleep(delay_seconds)
        crawls.append(await crawl_url(source.canonical_url, retries=retries))
    _write_pack(
        output,
        query,
        in_scope,
        selected,
        crawls,
        search_meta,
        excluded,
        canonical_unique_count=len(ranked),
    )
    return {
        "output": str(output),
        "discovered": search_meta.get("raw_result_count", len(search_results)),
        "unique": len(in_scope),
        "selected": len(selected),
        "processed": sum(item.success for item in crawls),
        "failed": sum(not item.success for item in crawls),
        "top_url": in_scope[0].canonical_url if in_scope else None,
        "top_tier": in_scope[0].tier if in_scope else None,
    }
