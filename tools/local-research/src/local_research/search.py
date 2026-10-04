from __future__ import annotations

import asyncio
from typing import Any

import httpx
from bs4 import BeautifulSoup

from .core import SearchResult


class SearchUnavailable(RuntimeError):
    pass


def _html_results(html: str, limit: int) -> list[SearchResult]:
    soup = BeautifulSoup(html, "html.parser")
    results: list[SearchResult] = []
    for article in soup.select("article.result"):
        link = article.select_one("h3 a[href]") or article.select_one("a.url_header[href]")
        if not link:
            continue
        url = str(link.get("href") or "").strip()
        if not url.startswith(("http://", "https://")):
            continue
        content = article.select_one("p.content") or article.select_one(".content")
        engines = [item.get_text(" ", strip=True) for item in article.select(".engines span, .engines")]
        results.append(
            SearchResult(
                url=url,
                title=link.get_text(" ", strip=True),
                snippet=content.get_text(" ", strip=True) if content else "",
                engine=", ".join(value for value in engines if value),
            )
        )
        if len(results) >= limit:
            break
    return results


async def search_searxng(
    query: str,
    base_url: str = "http://127.0.0.1:8888",
    limit: int = 20,
    retries: int = 2,
    timeout: float = 20.0,
) -> tuple[list[SearchResult], dict[str, Any]]:
    endpoint = base_url.rstrip("/") + "/search"
    last_error = "unknown search error"
    headers = {
        "User-Agent": "Mozilla/5.0 (Macintosh; Apple Silicon Mac OS X) LocalWebResearch/0.1",
        "Accept": "application/json",
    }
    async with httpx.AsyncClient(timeout=timeout, follow_redirects=True, headers=headers) as client:
        for attempt in range(retries + 1):
            try:
                response = await client.get(endpoint, params={"q": query, "format": "json", "language": "en"})
                if response.status_code == 200:
                    try:
                        payload = response.json()
                    except ValueError:
                        payload = None
                else:
                    payload = None
                if payload is None:
                    response = await client.get(endpoint, params={"q": query, "language": "en"})
                    response.raise_for_status()
                    html_items = _html_results(response.text, limit)
                    if not html_items:
                        raise ValueError("SearXNG returned HTML without parseable result articles")
                    return html_items, {
                        "endpoint": endpoint,
                        "response_format": "html",
                        "raw_result_count": len(html_items),
                        "returned_count": len(html_items),
                        "suggestions": [],
                        "attempts": attempt + 1,
                    }
                raw_results = payload.get("results", [])
                results = []
                for item in raw_results[:limit]:
                    url = str(item.get("url") or "").strip()
                    if not url:
                        continue
                    engines = item.get("engines") or item.get("engine") or []
                    if isinstance(engines, list):
                        engine = ", ".join(str(value) for value in engines)
                    else:
                        engine = str(engines)
                    published = item.get("publishedDate") or item.get("published_at")
                    results.append(
                        SearchResult(
                            url=url,
                            title=str(item.get("title") or ""),
                            snippet=str(item.get("content") or item.get("snippet") or ""),
                            engine=engine,
                            published_at=str(published) if published else None,
                        )
                    )
                return results, {
                    "endpoint": endpoint,
                    "response_format": "json",
                    "raw_result_count": len(raw_results),
                    "returned_count": len(results),
                    "suggestions": payload.get("suggestions", []),
                    "attempts": attempt + 1,
                }
            except (httpx.HTTPError, ValueError) as exc:
                last_error = f"{type(exc).__name__}: {exc}"
                if attempt < retries:
                    await asyncio.sleep(0.5 * (2**attempt))
    raise SearchUnavailable(f"SearXNG request failed at {endpoint}: {last_error}")
