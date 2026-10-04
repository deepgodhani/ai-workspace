#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


RESULTS = {
    "results": [
        {
            "url": "https://cloud.google.com/firestore/mongodb-compatibility/docs/overview",
            "title": "Firestore with MongoDB compatibility overview",
            "content": "Official Google Cloud documentation for Firestore Enterprise with MongoDB compatibility.",
            "engines": ["fixture-official-index"],
        },
        {
            "url": "https://cloud.google.com/firestore/mongodb-compatibility/docs/overview?utm_source=duplicate",
            "title": "Duplicate official result",
            "content": "Duplicate URL used to verify canonical deduplication.",
            "engines": ["fixture-duplicate-index"],
        },
        {
            "url": "https://no-such-host.invalid/firestore-compatibility",
            "title": "Unavailable technical source",
            "content": "An intentionally failed URL used to verify failure accounting.",
            "engines": ["fixture-failure-index"],
        },
        {
            "url": "https://example.com/top-10-firestore-alternatives?ref=affiliate",
            "title": "Top 10 Firestore alternatives and pricing",
            "content": "Sponsored buy now affiliate comparison.",
            "engines": ["fixture-promotional-index"],
        },
    ],
    "suggestions": [],
}


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        if self.path.startswith("/search"):
            body = json.dumps(RESULTS).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if self.path == "/config":
            body = b'{"instance_name":"SearXNG test fixture"}'
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_error(404)

    def log_message(self, format: str, *args: object) -> None:
        return


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 18888
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()

