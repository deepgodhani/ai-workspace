# Measurements

What this workspace claims, how each claim was measured, and what is not yet
proven. Reference numbers come from the author's own workspace (macOS 26.5,
Claude Code 2.1.289, Kiro CLI 2.27.1, Codex CLI 0.147.0), measured 2026-10-04.
Rerun everything on your own machine with the commands at the end.

## 1. It works from a fresh clone

`bin/selftest` copies the framework to a temp directory with an isolated
`HOME`, then exercises every helper.

| Check | Result |
|---|---|
| Skills link into Claude Code, Codex and Kiro, and resolve | pass |
| `new-project`, `new-research`, `new-agent-case` create complete scaffolds | pass |
| `workspace-context` resolves a target (exit 0) | pass |
| Root contract under 4 KB; `CLAUDE.md` imports `AGENTS.md` | pass |
| Kiro agent configs present; `workspace-doctor` runs | pass |
| `ws` builds from source and runs; `orch` dry run builds a context packet | pass |
| **Total** | **18 / 18** (2026-10-04, after the `ws` port) |

The first run failed one check: `workspace-context` printed correct output but
exited 1. The bug was fixed; this is what the self-test is for.

## 1a. The `bin/` tools as one C++ binary (`ws`)

On 2026-10-04 every `bin/` helper except the installers was ported to `ws`
(`tools/ws`), and the `bin/` names became 3-line wrappers. Method per tool: the
new black-box tests run against `ws` and, through a shim, against the original
script; on the real workspace the outputs were compared byte for byte; the
Python-based tools were also compared on random inputs. Full tables:
`tools/ws/docs/MEASUREMENTS.md`.

| | Before | After |
|---|---|---|
| End-to-end tests for `bin/` tools | 0 (only `bin/selftest`) | 71 (plus 34 for `ws orch`) |
| Old scripts passing the new tests | – | all except deliberate changes (`tools/ws/.ai/DECISIONS.md`) |
| `bin/` lines (wrappers + one private script kept in bash) | 952 | 95 |
| `bin/` tools needing Python / rsync | 6 / 2 | 0 / 0 |
| `export-oss --check` | 406 ms | 135 ms |
| `workspace-doctor` | 16.2–16.9 s | 16.8 s (`claude mcp list` alone takes ~16 s) |
| `token-usage`, `session-report` | ~0.7 s | ~0.7 s (`npx tokscale` dominates) |

Speed only changes where a script started Python or forked many processes; the
wrapper adds ~13–25 ms of bash start-up. Agent token spend per call is
unchanged: the output is the same text.

## 2. Context loaded per step vs context stored

Tokens estimated as bytes ÷ 4 (±20% for English Markdown; not a tokenizer).

| Step | Loaded |
|---|---|
| Always loaded: root `AGENTS.md` + `CLAUDE.md` | ~540 tokens |
| Plus the CLI's memory index (Claude Code) | ~990 tokens |
| Coach skill core, loaded when routing | ~1,490 tokens |
| Activating one target (its `AGENTS.md` + state/context files) | ~1,000–1,600 tokens |
| **Typical total to start work on a target** | **~4,500 tokens** |
| Text stored behind indexes (projects, research, knowledge, agents; excl. raw crawls) | ~17.8M tokens |

So routing loads about 1 token in 4,000 of what the workspace holds.

One target in the reference workspace loaded ~5,800 tokens because its
`STATE.md` was ~3× its 3–4 KB budget. Budgets only help if kept;
`workspace-context` warns above 12 KB.

**What this does not show:** the workspace's ~4.5k tokens are a small part of
a session's starting context. Short sessions in the logs below started around
~75k tokens per turn, mostly the CLI's own system prompt and tool definitions.
The workspace keeps its share small; it cannot shrink the CLI's.

## 3. What actually drives spend: session length

From local Claude Code logs (`bin/session-report`, 101 sessions,
2026-08-01 → 2026-10-04, cost at API list price):

| Messages per session | Sessions | Median tokens re-read per message | Share of cost |
|---|---|---|---|
| 1–20 | 39 | 75,551 | 1.1% |
| 21–50 | 21 | 137,670 | 3.1% |
| 51–100 | 8 | 139,968 | 2.3% |
| 101–200 | 6 | 212,021 | 4.1% |
| 201–400 | 15 | 306,688 | 32.6% |
| 401–800 | 10 | 430,450 | 42.6% |
| 801+ | 2 | 466,054 | 14.2% |

Correlation between log(session length) and tokens per message: r = 0.79.
**27 sessions over 200 messages produced 89% of the cost.** ≈98% of all tokens
were cache reads, so caching already works; the cost is the size of what is
re-read on every turn.

Sessions started at the workspace root averaged ~352k tokens per message;
sessions started inside a project repository averaged ~387k. Working from the
root did not make turns heavier.

## 4. On paper: what splitting long sessions would save

The workspace's handoff → fresh session cycle exists to keep sessions short.
A simple cost model:

- **Saving per message after a split:** a 401–800-message session re-reads
  ~430k tokens per turn; a fresh session in its first ~100 turns re-reads
  ~140k. Δ ≈ 290k cache-read tokens per message.
- **Cost of a split:** the new session writes its starting prefix (S ≈ 75k
  tokens) to cache instead of reading it. Using Anthropic's published
  multipliers (cache write 2× input for a 1-hour TTL; cache read 0.1× input,
  0.05× on Opus 5.5), that costs S × (w − r) / r ≈ 1.4M–2.9M cache-read-token
  equivalents.
- **Break-even:** ≈ 5–10 messages after the split.

If every session over 200 messages had been split so its turns re-read about
the 101–200 bucket median (~212k), those sessions would cost ~31%, ~51% and
~55% less per bucket, about **40% of total Claude Code spend**, with restart
overhead negligible next to that.

**This is a model, not a result.** It assumes per-turn context in split
sessions behaves like the shorter sessions observed. That is plausible, but
the data is observational: long sessions may also be harder tasks.

## 5. Not proven

- **Quality.** Nothing here measures whether shorter sessions or this layout
  produce equally good work. Handoffs lose detail; compaction is weakest at
  remembering which files changed. Proving "no worse outcome" needs paired
  runs: the same tasks, done both ways, scored by their acceptance checks.
- **Causality.** Section 3 is correlation from one person's logs.
- **Kiro and Codex.** Their local logs record few or no cache reads, so their
  numbers are not comparable with Claude Code's.
- **Token counts** are byte estimates (section 2) and tokscale price estimates
  (sections 3–4), not invoices.

## Reproduce

```bash
bin/selftest                                  # section 1
bin/workspace-context <target>                # per-target startup bytes (section 2)
bin/session-report --since 2026-08-01         # section 3 (reads local logs; uploads nothing)
bin/token-usage --by-session --week           # per-session totals for before/after comparisons
```
