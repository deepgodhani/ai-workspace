# Troubleshooting

- Run `~/Workspace/bin/workspace-doctor` for workspace and tool discovery.
- Run `~/Workspace/tools/local-research/bin/status` for SearXNG, Crawl4AI, and
  Python crawler status.
- If local research is stopped, run `bin/start` from its tool directory.
- If the browser is missing, run `bin/setup` again.
- If search JSON returns 403, confirm JSON output is enabled in the local
  SearXNG settings.
- If a database port conflicts, inspect
  `tools/database-port-registry.yaml`; reserve a new unassigned port, then
  update the selected case manifest and ignored `.env` together. Never
  silently repurpose another owner's port.
- Preserve failed URL evidence in `GAPS.md` and `COVERAGE.json`.
- Never put credentials in tracked settings, prompts, or reports.
- Firecrawl configuration is optional and should not mask a broken local setup.
