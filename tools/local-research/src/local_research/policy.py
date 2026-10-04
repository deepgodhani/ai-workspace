from __future__ import annotations

import fnmatch
import math
import re
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import urlsplit

from .core import SearchResult, canonicalize_url, load_yaml


def _domain_path(url: str) -> str:
    parts = urlsplit(url)
    return f"{parts.hostname or ''}{parts.path}".lower().rstrip("/")


def _matches(value: str, pattern: str) -> bool:
    pattern = pattern.lower().strip().rstrip("/")
    if "*" in pattern:
        return fnmatch.fnmatch(value, pattern)
    return value == pattern or value.startswith(pattern + "/") or value.endswith("." + pattern)


class SourcePolicy:
    def __init__(self, config_dir: Path):
        self.policy = load_yaml(config_dir / "source-policy.yml")
        self.domains = load_yaml(config_dir / "domains.yml")

    def allowed(self, url: str) -> tuple[bool, str]:
        value = _domain_path(url)
        for pattern in self.domains.get("deny", []):
            if _matches(value, str(pattern)):
                return False, f"denied by domain policy: {pattern}"
        allow = self.domains.get("allow", [])
        if allow and not any(_matches(value, str(pattern)) for pattern in allow):
            return False, "not in domain allow list"
        return True, "allowed"

    def tier_for(self, url: str) -> tuple[str, str]:
        value = _domain_path(url)
        for tier in ("A", "B", "C", "D", "E"):
            for pattern in self.domains.get("tiers", {}).get(tier, []):
                if _matches(value, str(pattern)):
                    return tier, f"matched {pattern}"
        host = (urlsplit(url).hostname or "").lower()
        if host.endswith(".gov") or host.endswith(".gov.in") or host.endswith(".edu"):
            return "A", "government or academic domain"
        if host.startswith("docs.") or host.startswith("developer."):
            return "B", "documentation subdomain"
        return "C", "unclassified independent source"

    def rank(self, results: list[SearchResult], query: str, version: str | None = None) -> list[SearchResult]:
        tokens = {token for token in re.findall(r"[a-z0-9]+", query.lower()) if len(token) > 2}
        versions = set(re.findall(r"\b(?:v?\d+(?:\.\d+){0,2}|20\d{2})\b", query.lower()))
        if version:
            versions.add(version.lower())
        tiers = self.policy.get("tiers", {})
        scoring = self.policy.get("scoring", {})
        promo_markers = [str(item).lower() for item in self.policy.get("promotional_markers", [])]

        deduped: dict[str, SearchResult] = {}
        for result in results:
            try:
                result.canonical_url = canonicalize_url(result.url)
            except ValueError:
                continue
            if result.canonical_url in deduped:
                continue
            deduped[result.canonical_url] = result

            result.tier, tier_reason = self.tier_for(result.canonical_url)
            result.authority_score = float(tiers.get(result.tier, {}).get("score", 0))
            result.score_reasons.append(f"tier {result.tier}: {tier_reason}")

            haystack = f"{result.title} {result.snippet} {result.canonical_url}".lower()
            overlap = len(tokens & set(re.findall(r"[a-z0-9]+", haystack)))
            result.relevance_score = round(min(float(scoring.get("relevance_max", 30)), overlap * 4.0), 2)

            path = urlsplit(result.canonical_url).path.lower()
            if any(marker in path for marker in ("/docs", "/documentation", "/reference", "/api", "/spec")):
                result.directness_score = float(scoring.get("directness_max", 8))
                result.score_reasons.append("direct documentation path")

            if versions and any(item in haystack for item in versions):
                result.version_score = float(scoring.get("version_match_max", 7))
                result.score_reasons.append("version matched")

            if result.published_at:
                try:
                    parsed = datetime.fromisoformat(result.published_at.replace("Z", "+00:00"))
                    age_days = max(0, (datetime.now(timezone.utc) - parsed.astimezone(timezone.utc)).days)
                    result.freshness_score = round(float(scoring.get("freshness_max", 10)) * math.exp(-age_days / 730), 2)
                except (ValueError, TypeError):
                    pass

            if any(marker in haystack for marker in promo_markers):
                result.promotional_penalty = float(scoring.get("promotional_penalty", 20))
                result.score_reasons.append("promotional language penalty")

            result.total_score = round(
                result.authority_score
                + result.relevance_score
                + result.freshness_score
                + result.directness_score
                + result.version_score
                - result.promotional_penalty,
                2,
            )

        return sorted(deduped.values(), key=lambda item: (-item.total_score, item.canonical_url))

