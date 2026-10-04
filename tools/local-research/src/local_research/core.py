from __future__ import annotations

import hashlib
import json
import re
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.parse import parse_qsl, urlencode, urlsplit, urlunsplit

import yaml

TRACKING_KEYS = {
    "fbclid",
    "gclid",
    "mc_cid",
    "mc_eid",
    "ref",
    "ref_src",
}
TRACKING_PREFIXES = ("utm_",)


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def canonicalize_url(url: str) -> str:
    parts = urlsplit(url.strip())
    if parts.scheme.lower() not in {"http", "https"} or not parts.hostname:
        raise ValueError(f"Unsupported or incomplete URL: {url}")
    host = parts.hostname.lower()
    port = parts.port
    netloc = host
    if port and not ((parts.scheme.lower() == "http" and port == 80) or (parts.scheme.lower() == "https" and port == 443)):
        netloc = f"{host}:{port}"
    clean_query = [
        (key, value)
        for key, value in parse_qsl(parts.query, keep_blank_values=True)
        if key.lower() not in TRACKING_KEYS
        and not any(key.lower().startswith(prefix) for prefix in TRACKING_PREFIXES)
    ]
    path = re.sub(r"/{2,}", "/", parts.path or "/")
    if path != "/":
        path = path.rstrip("/")
    return urlunsplit((parts.scheme.lower(), netloc, path, urlencode(clean_query, doseq=True), ""))


def safe_stem(url: str) -> str:
    parts = urlsplit(url)
    label = re.sub(r"[^a-z0-9]+", "-", f"{parts.hostname}-{parts.path}".lower()).strip("-")
    digest = hashlib.sha256(url.encode("utf-8")).hexdigest()[:10]
    return f"{label[:70] or 'page'}-{digest}"


def load_yaml(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return yaml.safe_load(handle) or {}


@dataclass
class SearchResult:
    url: str
    title: str = ""
    snippet: str = ""
    engine: str = ""
    published_at: str | None = None
    canonical_url: str = ""
    tier: str = "E"
    authority_score: float = 0.0
    relevance_score: float = 0.0
    freshness_score: float = 0.0
    directness_score: float = 0.0
    version_score: float = 0.0
    promotional_penalty: float = 0.0
    total_score: float = 0.0
    score_reasons: list[str] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class CrawlResult:
    url: str
    success: bool
    method: str
    retrieved_at: str
    status_code: int | None = None
    markdown: str = ""
    title: str = ""
    final_url: str = ""
    error: str = ""
    attempts: int = 1
    warnings: list[str] = field(default_factory=list)

    def to_dict(self, include_markdown: bool = True) -> dict[str, Any]:
        value = asdict(self)
        if not include_markdown:
            value.pop("markdown", None)
        return value


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
