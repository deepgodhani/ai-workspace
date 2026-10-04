from __future__ import annotations

import argparse
import asyncio
import json
import os
from pathlib import Path

from .crawl import crawl_url
from .policy import SourcePolicy
from .research import run_research
from .search import SearchUnavailable, search_searxng


def _tool_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _searxng_url() -> str:
    return os.environ.get("SEARXNG_URL", "http://127.0.0.1:8888")


async def _search(args: argparse.Namespace) -> int:
    results, meta = await search_searxng(args.query, args.searxng_url, limit=args.limit)
    ranked = SourcePolicy(args.config_dir).rank(results, args.query, version=args.version)
    payload = {"query": args.query, "meta": meta, "results": [item.to_dict() for item in ranked]}
    print(json.dumps(payload, indent=2, ensure_ascii=False))
    return 0 if ranked else 2


def search_main() -> None:
    parser = argparse.ArgumentParser(description="Search local SearXNG and rank canonical sources.")
    parser.add_argument("query")
    parser.add_argument("--limit", type=int, default=20)
    parser.add_argument("--version")
    parser.add_argument("--searxng-url", default=_searxng_url())
    parser.add_argument("--config-dir", type=Path, default=_tool_root() / "config")
    try:
        code = asyncio.run(_search(parser.parse_args()))
    except SearchUnavailable as exc:
        print(json.dumps({"error": str(exc)}, indent=2))
        code = 2
    raise SystemExit(code)


async def _crawl(args: argparse.Namespace) -> int:
    result = await crawl_url(args.url, retries=args.retries, timeout_ms=args.timeout * 1000)
    if args.output and result.success:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(result.markdown.rstrip() + "\n", encoding="utf-8")
    payload = result.to_dict(include_markdown=not bool(args.output))
    if args.output:
        payload["output"] = str(args.output)
    print(json.dumps(payload, indent=2, ensure_ascii=False))
    return 0 if result.success else 2


def crawl_main() -> None:
    parser = argparse.ArgumentParser(description="Crawl one URL with Crawl4AI and browser/HTTP fallbacks.")
    parser.add_argument("url")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--retries", type=int, default=1)
    parser.add_argument("--timeout", type=int, default=45, help="Per-attempt timeout in seconds")
    raise SystemExit(asyncio.run(_crawl(parser.parse_args())))


def research_main() -> None:
    parser = argparse.ArgumentParser(description="Run local search, ranking, crawl, and research-pack generation.")
    parser.add_argument("query")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-results", type=int, default=20)
    parser.add_argument("--max-pages", type=int, default=5)
    parser.add_argument("--delay", type=float, default=1.0)
    parser.add_argument("--retries", type=int, default=1)
    parser.add_argument("--version")
    parser.add_argument("--searxng-url", default=_searxng_url())
    parser.add_argument("--config-dir", type=Path, default=_tool_root() / "config")
    args = parser.parse_args()
    try:
        result = asyncio.run(
            run_research(
                query=args.query,
                output=args.output,
                config_dir=args.config_dir,
                searxng_url=args.searxng_url,
                max_results=args.max_results,
                max_pages=args.max_pages,
                delay_seconds=args.delay,
                retries=args.retries,
                version=args.version,
            )
        )
    except SearchUnavailable as exc:
        print(json.dumps({"error": str(exc)}, indent=2))
        raise SystemExit(2)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    raise SystemExit(0 if result["processed"] else 2)
