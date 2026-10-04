# Context Budget

Use progressive disclosure so stored information remains large while active
context remains small.

## Default tiers

1. Instructions: applicable `AGENTS.md`.
2. Startup: current state, active tasks, compact index.
3. Targeted context: selected case status, one knowledge note, one research
   context, or a searched decision section.
4. Detailed evidence: a specific report section or generated summary.
5. Raw evidence: logs, exports, datasets, crawls, `.runtime/`, `inputs/`, or
   `raw/`; load only to answer a concrete unresolved question.

## Recommended budgets

- Root or target `AGENTS.md`: up to 2 KB when practical.
- Skill core: 3–5 KB; move conditional workflows to references.
- `STATE.md`: 3–4 KB.
- `TASKS.md`: 2 KB.
- `INDEX.md`: 2–3 KB.
- Case `STATUS.md`: 3 KB.
- Root activation or execution packet: no more than about 800 tokens.

These are routing budgets, not hard limits for evidence files.

## Compaction rules

- Keep only current facts in state.
- Move completed history to dated session notes or case reports.
- Keep only active and immediately next tasks in task files.
- Search append-only decisions instead of reading the whole file.
- Link to reports rather than repeating their conclusions in multiple files.
- Store one-line case and project entries in indexes.
- Never copy terminal transcripts or full chat histories into startup context.

Large files on disk consume no model tokens until opened or injected. The risk
comes from broad scans, oversized tool output, attachments, or instructions
that say to read a whole directory. Prefer bounded searches, headings, counts,
and exact paths.

## Token efficiency (evidence: research/topics/agent-token-efficiency)

Spend is mostly the same context re-read every turn (≈98% of tokens here are
cache reads), so fewer turns and smaller context beat clever compression.
Only rules that are lossless or quality-measured belong here.

- **Keep the cache warm.** Don't switch model, effort, MCP servers, or
  cwd/approval mode mid-session; each forces a full-price re-read.
- **One task per conversation.** `/clear` (free) or handoff → fresh chat
  between unrelated tasks; don't let one session drift across targets.
- **Bound what enters context.** Search, then read ranges, not whole files;
  bounded views beat full-file reads in SWE-agent evals.
- **Long command output → file.** Redirect verbose build/test/log output to a
  scratch file and grep/tail it; the full output stays restorable. Don't use
  lossy output filters (rtk-style hooks measured +7.6% cost).
- **Compaction is lossy.** If you `/compact`, say what to keep: touched files,
  failing tests, decisions, exact next action. File tracking is what
  compaction loses most, so keep it in `STATE.md` too.
- **Tool search on; fewer servers.** Prefer `gh`/`gcloud`/`aws` CLIs over MCP
  servers; prune unused servers between sessions, not during one.
- **Subagents isolate, they don't save.** Agents ≈4× and multi-agent ≈15×
  chat tokens; use them for focus/quality, not to cut spend.
- **Not adopted (lossy or unproven):** LLMLingua-style compression of agent
  context, code minification, semantic-code MCPs without a local A/B,
  lower effort/model by default.
- **Measure, don't trust counters.** `bin/token-usage --by-session` before and
  after; compare paired runs of the same task, never single runs.

## Conversation lifecycle

Handoff preserves durable context; a fresh root chat discards accumulated chat
history. The user never needs to change directories:

`root chat → activate target → work → handoff → fresh root chat → resume`
