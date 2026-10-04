from pathlib import Path

from local_research.core import SearchResult, canonicalize_url
from local_research.policy import SourcePolicy
from local_research.search import _html_results
from local_research.crawl import _extract_static_markdown


CONFIG = Path(__file__).resolve().parents[1] / "config"


def test_canonicalize_removes_fragment_tracking_and_default_port():
    assert canonicalize_url("HTTPS://Example.COM:443/a//b/?utm_source=x&b=2#part") == "https://example.com/a/b?b=2"


def test_ranking_deduplicates_and_prefers_official_docs():
    policy = SourcePolicy(CONFIG)
    ranked = policy.rank(
        [
            SearchResult(
                url="https://cloud.google.com/firestore/docs/enterprise/behavior-differences?utm_source=test",
                title="Behavior differences in Firestore Enterprise",
                snippet="Official MongoDB compatibility documentation",
            ),
            SearchResult(
                url="https://cloud.google.com/firestore/docs/enterprise/behavior-differences",
                title="Duplicate",
            ),
            SearchResult(
                url="https://example.com/top-10-firestore-alternatives",
                title="Top 10 Firestore alternatives pricing",
                snippet="Buy now",
            ),
        ],
        "Firestore Enterprise MongoDB compatibility official documentation",
    )
    assert len(ranked) == 2
    assert ranked[0].tier == "A"
    assert ranked[0].canonical_url.startswith("https://cloud.google.com/")
    assert ranked[0].total_score > ranked[1].total_score


def test_domain_policy_denies_local_addresses():
    policy = SourcePolicy(CONFIG)
    allowed, reason = policy.allowed("http://127.0.0.1/private")
    assert not allowed
    assert "denied" in reason


def test_searxng_html_fallback_parser():
    html = """
    <article class="result result-default">
      <h3><a href="https://docs.example.com/guide">Official guide</a></h3>
      <p class="content">Primary documentation.</p>
    </article>
    """
    parsed = _html_results(html, 10)
    assert len(parsed) == 1
    assert parsed[0].url == "https://docs.example.com/guide"
    assert parsed[0].snippet == "Primary documentation."


def test_static_extraction_prefers_article_over_site_chrome():
    html = """
    <html><body><nav>Product navigation and sign in</nav>
    <article><h1>Research finding</h1><p>Useful primary-source evidence.</p></article>
    <footer>Copyright and newsletter</footer></body></html>
    """
    extracted = _extract_static_markdown(html)
    assert "Research finding" in extracted
    assert "Useful primary-source evidence" in extracted
    assert "Product navigation" not in extracted
