from __future__ import annotations

import asyncio
import os
import urllib.robotparser
from pathlib import Path
from typing import Awaitable, Callable

import httpx
import trafilatura
from bs4 import BeautifulSoup
from markdownify import markdownify

from .core import CrawlResult, utc_now

USER_AGENT = "LocalWebResearch/0.1 (+local, respectful research crawler)"


def _clean_markdown(html: str) -> str:
    soup = BeautifulSoup(html, "html.parser")
    for tag in soup.select("script, style, nav, header, footer, aside, noscript"):
        tag.decompose()
    content = soup.select_one("main") or soup.select_one("article") or soup.body or soup
    return markdownify(str(content), heading_style="ATX").strip()


def _extract_static_markdown(html: str) -> str:
    """Prefer main-content extraction before retaining a lightweight HTML fallback."""
    extracted = trafilatura.extract(
        html,
        output_format="markdown",
        include_links=True,
        include_images=False,
        include_comments=False,
        favor_precision=True,
        deduplicate=True,
    )
    return (extracted or _clean_markdown(html)).strip()


def _compact_error(exc: Exception, limit: int = 500) -> str:
    text = str(exc).split("Browser logs:", 1)[0]
    text = " ".join(text.split())
    return f"{type(exc).__name__}: {text}"[:limit]


async def _robots_allowed(url: str, timeout: float = 10.0) -> tuple[bool, str]:
    robots_url = httpx.URL(url).copy_with(path="/robots.txt", query=None, fragment=None)
    try:
        async with httpx.AsyncClient(timeout=timeout, follow_redirects=True, headers={"User-Agent": USER_AGENT}) as client:
            response = await client.get(robots_url)
        if response.status_code >= 400:
            return True, f"robots unavailable ({response.status_code}); site policy still applies"
        parser = urllib.robotparser.RobotFileParser()
        parser.set_url(str(robots_url))
        parser.parse(response.text.splitlines())
        allowed = parser.can_fetch(USER_AGENT, url)
        return allowed, "allowed by robots.txt" if allowed else "disallowed by robots.txt"
    except httpx.HTTPError as exc:
        return True, f"robots check failed ({exc}); site policy still applies"


def _markdown_value(value: object) -> str:
    if isinstance(value, str):
        return value
    raw = getattr(value, "raw_markdown", None)
    if isinstance(raw, str):
        return raw
    fit = getattr(value, "fit_markdown", None)
    if isinstance(fit, str):
        return fit
    return str(value or "")


async def _crawl4ai(url: str, timeout_ms: int) -> CrawlResult:
    work_root = Path(os.environ.get("LOCAL_RESEARCH_WORK_DIR", Path.cwd() / "work"))
    work_root.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("CRAWL4_AI_BASE_DIRECTORY", str(work_root))
    from crawl4ai import AsyncWebCrawler, BrowserConfig, CacheMode, CrawlerRunConfig

    browser = BrowserConfig(browser_type="chromium", headless=True, verbose=False)
    run = CrawlerRunConfig(
        cache_mode=CacheMode.BYPASS,
        check_robots_txt=True,
        page_timeout=timeout_ms,
        wait_until="domcontentloaded",
        word_count_threshold=10,
        excluded_tags=["nav", "footer", "aside"],
    )
    work_dir = work_root / "crawl4ai"
    work_dir.mkdir(parents=True, exist_ok=True)
    async with AsyncWebCrawler(config=browser, base_directory=str(work_dir)) as crawler:
        result = await crawler.arun(url=url, config=run)
    markdown = _markdown_value(getattr(result, "markdown", ""))
    success = bool(getattr(result, "success", False)) and bool(markdown.strip())
    return CrawlResult(
        url=url,
        success=success,
        method="crawl4ai",
        retrieved_at=utc_now(),
        status_code=getattr(result, "status_code", None),
        markdown=markdown,
        title=str((getattr(result, "metadata", {}) or {}).get("title", "")),
        final_url=str(getattr(result, "redirected_url", "") or getattr(result, "url", "") or url),
        error="" if success else str(getattr(result, "error_message", "empty Markdown result")),
    )


async def _playwright(url: str, timeout_ms: int) -> CrawlResult:
    from playwright.async_api import async_playwright

    async with async_playwright() as playwright:
        browser = await playwright.chromium.launch(headless=True)
        try:
            page = await browser.new_page(user_agent=USER_AGENT)
            response = await page.goto(url, wait_until="domcontentloaded", timeout=timeout_ms)
            html = await page.content()
            title = await page.title()
            final_url = page.url
        finally:
            await browser.close()
    clean = _clean_markdown(html)
    return CrawlResult(
        url=url,
        success=bool(clean),
        method="playwright-fallback",
        retrieved_at=utc_now(),
        status_code=response.status if response else None,
        markdown=clean,
        title=title,
        final_url=final_url,
        error="" if clean else "empty Markdown result",
    )


async def _http_fallback(url: str, timeout_ms: int) -> CrawlResult:
    async with httpx.AsyncClient(
        timeout=timeout_ms / 1000,
        follow_redirects=True,
        headers={"User-Agent": USER_AGENT, "Accept": "text/html,application/xhtml+xml"},
    ) as client:
        response = await client.get(url)
        response.raise_for_status()
    soup = BeautifulSoup(response.text, "html.parser")
    title = soup.title.get_text(" ", strip=True) if soup.title else ""
    clean = _extract_static_markdown(response.text)
    return CrawlResult(
        url=url,
        success=bool(clean),
        method="trafilatura-http-fallback",
        retrieved_at=utc_now(),
        status_code=response.status_code,
        markdown=clean,
        title=title,
        final_url=str(response.url),
        error="" if clean else "empty Markdown result",
    )


async def crawl_url(url: str, retries: int = 1, timeout_ms: int = 45_000) -> CrawlResult:
    allowed, robots_reason = await _robots_allowed(url)
    if not allowed:
        return CrawlResult(
            url=url,
            success=False,
            method="policy",
            retrieved_at=utc_now(),
            error=robots_reason,
            attempts=0,
        )

    methods: list[tuple[str, Callable[[str, int], Awaitable[CrawlResult]]]] = [
        ("crawl4ai", _crawl4ai),
        ("http-fallback", _http_fallback),
        ("playwright-fallback", _playwright),
    ]
    failures: list[str] = []
    attempts = 0
    for name, method in methods:
        for retry in range(retries + 1):
            attempts += 1
            try:
                result = await method(url, timeout_ms)
                result.attempts = attempts
                if result.success:
                    result.warnings = failures.copy()
                    return result
                failures.append(f"{name}: {result.error or 'unsuccessful result'}")
            except Exception as exc:  # Each fallback must remain isolated.
                failures.append(f"{name}: {_compact_error(exc)}")
            if retry < retries:
                await asyncio.sleep(0.75 * (2**retry))
    return CrawlResult(
        url=url,
        success=False,
        method="all-fallbacks-failed",
        retrieved_at=utc_now(),
        error="; ".join(failures),
        attempts=attempts,
    )
