# DECISIONS — tools/ws

- 2026-10-04 — **C++20 for all workspace tools** (user decision; Go/Rust were
  offered). One binary, `ws`, with subcommands; `bin/` names stay as wrappers.
- 2026-10-04 — **nlohmann/json fetched at build time, SHA-256 pinned** (matches
  the release notes); never vendored. SQLite from the system.
- 2026-10-04 — **Black-box tests carried over from the TypeScript version** as the
  spec; Node is a dev-only dependency for the runner.
- 2026-10-04 — **Shell allow-list matches every chained segment**; substitution
  and redirection always denied (found by a real Kiro run).
- 2026-10-04 — **Ports keep output and exit codes, but fix accidental side
  effects.** `ws new agent-case` keeps template file modes; the bash script left
  every substituted text file at 0600 (mktemp + mv). Usage lines say `ws <sub>`
  instead of `$0`; `-h/--help` added. A missing `git` is tolerated as before.
- 2026-10-04 — **Remaining bin/ scripts are in scope** (user decision): also
  the private case script, configure-firecrawl, firecrawl-mcp-wrapper, and both
  installers. Installers can't be plain wrappers (they run before `ws` is built);
  proposal in docs/MEASUREMENTS.md §6 awaits the user's OK.
- 2026-10-04 — `ws link-skills` creates nothing when there are no skills (the bash
  glob made a dangling `.kiro/skills/*` link). Arguments other than --help are
  still ignored, as before.
- 2026-10-04 — **Private checks move out of C++ into the bin/ wrapper.** `export-oss`
  strips `# private:` blocks only in text files, and `tools/ws` is exported whole, so
  private names must never be in `src/` or `test/`. `ws doctor` takes generic
  `--agent-readme NAME` / `--check-exec PATH`; `bin/workspace-doctor` passes the
  private ones inside a `# private:` block (export removes them, as before). The same
  rule applies to every later port.
- 2026-10-04 — `ws doctor` counts a tool as present only if PATH holds an
  *executable* file (bash `command -v` also reported non-executable files as ok);
  `--help` added; unset HOME still stops after "Local research" with exit 1. Child
  output (status script, `codex/claude mcp list`) runs on the inherited terminal, as
  in the script.
- 2026-10-04 — `ws selftest` copies the workspace itself (rsync dropped; same excludes:
  a bare name matches at any depth, a path rule matches trailing components) and
  clears `WS_ROOT`/`ORCH_WORKSPACE` so tools in the copy can never resolve to the
  source workspace (the bin/ wrapper sets `WS_ROOT`, which would otherwise leak).
  The temp folder honours `$TMPDIR` (macOS `mktemp -d` ignored it). `--help` added.
  `bin/selftest` now needs `ws` built in the source workspace (`bin/ws` says so);
  the script built only inside the copy.
- 2026-10-04 — `ws export-oss` keeps the Python script's behaviour as run by the
  installed Python 3.14 (e.g. `Path.suffix` of `foo.` is `.`): manifest rules, skip
  rules, private-marker stripping (`re.sub` semantics, universal newlines), generated
  scaffolding, the scan (all rules, `\b` and IGNORECASE on code points, `splitlines`
  line numbers, 50-hit cap), the symlink-escape guard (a port of Python's non-strict
  `realpath`), and `rsync -a --delete --exclude .git` into DEST. Checked by 8 tests the
  old script also passes, byte-identical `--check` and export trees on the real
  workspace, and a differential fuzz (300 runs × 24 random lines, 0 differences).
  Deviations: usage says `ws export-oss`; rsync's "cannot delete non-empty directory"
  warning has ws wording; `\b`/case folding of non-ASCII letters use the C library's
  Unicode tables plus Python's extra equivalences.
- 2026-10-04 — **Found: the published export-oss was broken.** Its own `MD_BLOCK`
  regex source matches the private-block pattern, so the exported copy had that line
  stripped to `[ \t]*[ \t]*\n?`, which deletes whitespace in any file containing a
  private marker. The port removes the problem (C++ sources are not stripped; the
  wrapper holds no markers). Takes effect at the next export.
- 2026-10-04 — `ws usage` / `ws session-report` keep calling `npx -y tokscale@4.17.0
  models --json …` with the same arguments (TOKSCALE_VERSION still overrides for
  `usage`) and reproduce the Python reports digit for digit: format() rules
  (padding by code point, `,` grouping, float repr), stable cost sort, negative
  `--top` as a slice, int/int true division rounded once, `sum()` as Python 3.12+
  Neumaier, `statistics.mean` exact. Checked by 13 tests (old scripts 12/13), real
  tokscale output (identical), and a differential fuzz (900 runs, 0 differences
  after fixing two bugs it found). Deviations: `ws usage --help` prints to stdout
  and exits 0 (the script printed usage to stderr, exit 2); errors that were Python
  tracebacks (bad `--top`, tokscale failure or missing npx in session-report, bad
  JSON) are one-line messages with the same exit code 1; session-report shows
  tokscale's stderr when it fails.
- 2026-10-04 — `ws firecrawl configure` / `ws firecrawl mcp` keep the credential file
  format (`export FIRECRAWL_API_KEY=<bash 3.2 printf %q>`) and path, so old and new
  files work with both versions (checked on the real file). Messages, exit codes, and
  stdin handling (`read -r -s -p`, one byte at a time) match; `mcp` replaces itself
  with `npx -y firecrawl-mcp` like `exec`. Deliberate changes: the file is created at
  0600 before the key is written (the script wrote first, then chmod); `mcp` parses
  the file as `[export] NAME=value` lines (quotes, `\`, `$'…'`) and refuses anything
  else instead of running it as a shell script; `--help` added. 12 tests; old 10/12.
- 2026-10-04 — **the private case script stays bash** (user accepted the
  recommendation). It is private: not in the export manifest, it names a private
  agent, and it calls that agent's own `mfv` tool. Porting it would put private
  names into exported C++ or need a generic command with one private user.
- 2026-10-04 — **`_shared/oss/` is exported** (user: "complete the open parts"). The
  script skipped it, so `bin/export-oss` failed in every clone (no manifest or
  templates). Its files are copied verbatim (no private-marker stripping: the README
  quotes the markers). The old script fails 5 of the 9 export tests, all only
  because of these 7 extra files (count and listing); a test exports a clone again.
- 2026-10-04 — **Installers** (user approved the smaller plan instead of §6's
  `ws init`). `bin/setup-ai-workspace-full.sh` (private, 5,443 lines with 99 embedded
  files, including outdated copies of 7 bin/ tools) is now a 209-line bootstrap that
  installs the exported framework (`--source DIR|URL`, default the public repo),
  builds `ws`, runs `link-skills` and `workspace-doctor`; same options and
  never-overwrite rule; the old file is `setup-ai-workspace-full.sh.bak`.
  `bin/init-ai-workspace.sh` is unchanged: it creates a different, self-contained
  portable workspace (`.workspace/`, `./workspace` launcher) with no `tools/ws`.
  Verified on throwaway folders: fresh install (201 files, selftest 18/18 in the
  result), rerun keeps everything (0 created), Git-URL source, bad options exit 2.
