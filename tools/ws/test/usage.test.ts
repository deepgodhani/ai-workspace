// Black-box tests for `ws usage` and `ws session-report` (ports of bin/token-usage and
// bin/session-report). A fake `npx` records its arguments and prints a tokscale fixture.
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { chmodSync, existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { afterEach, beforeEach, describe, test } from "node:test";

const ROOT = resolve(import.meta.dirname, "..");
const WS_BIN = process.env.WS_BIN ?? join(ROOT, "build", "ws");
let base: string;
let fakebin: string;
let argsLog: string;
let fixture: string;

function ws(args: string[], env: Record<string, string> = {}) {
  const e: Record<string, string> = { PATH: `${fakebin}:/usr/bin:/bin`, HOME: base, FIXTURE: fixture, ARGS_LOG: argsLog, ...env };
  const r = spawnSync(WS_BIN, args, { cwd: base, encoding: "utf8", env: e });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

const npxArgs = () => (existsSync(argsLog) ? readFileSync(argsLog, "utf8").trimEnd() : null);

function fakeNpx(body: string) {
  writeFileSync(join(fakebin, "npx"), `#!/bin/sh\nprintf '%s\\n' "$*" > "$ARGS_LOG"\n${body}\n`);
  chmodSync(join(fakebin, "npx"), 0o755);
}

// Exercises: cost order with a stable tie, model/session truncation, the `session`
// fallback for an empty sessionId, a missing sessionId and cost, thousands separators.
const FIXTURE = {
  groupBy: "client,session,model",
  entries: [
    { client: "claude", sessionId: "aaaaaaaa-1111-2222", model: "claude-opus-5-5", input: 1200, output: 3400, cacheRead: 980000, cacheWrite: 15400, messageCount: 12, cost: 1.25 },
    { client: "codex", sessionId: "bbbbbbbb-3333", model: "gpt-5.6-terra-extra-long-model-name", input: 50000, output: 2000, cacheRead: 0, cacheWrite: 0, messageCount: 30, cost: 4.5 },
    { client: "kiro", sessionId: "", session: "kiro-session-xyz-123", model: "auto", input: 7000, output: 0, cacheRead: 0, cacheWrite: 0, messageCount: 3, cost: 0.0 },
    { client: "claude", sessionId: "cccccccc-4444", model: "claude-opus-5-5", input: 10, output: 250000, cacheRead: 120000000, cacheWrite: 1000000, messageCount: 250, cost: 38.94 },
    { client: "claude", sessionId: "aaaaaaaa-1111-2222", model: "claude-haiku-5", input: 100, output: 200, cacheRead: 300, cacheWrite: 400, messageCount: 5, cost: 1.25 },
    { client: "gemini", model: "gemini-3-pro", input: 0, output: 0, cacheRead: 0, cacheWrite: 0, messageCount: 0 },
  ],
  totalInput: 58310,
  totalOutput: 255600,
  totalCacheRead: 120980300,
  totalCacheWrite: 1015800,
  totalMessages: 300,
  totalCost: 45.94,
  processingTimeMs: 5,
};

const HEADER = "client   model                  session        msgs         tokens cache-read%        out     cost$\n";
const ROWS = [
  "claude   claude-opus-5-5        cccccccc-444    250    121,250,010       99.0%    250,000     38.94\n",
  "codex    gpt-5.6-terra-extra-lo bbbbbbbb-333     30         52,000        0.0%      2,000      4.50\n",
  "claude   claude-opus-5-5        aaaaaaaa-111     12      1,000,000       98.0%      3,400      1.25\n",
  "claude   claude-haiku-5         aaaaaaaa-111      5          1,000       30.0%        200      1.25\n",
  "kiro     auto                   kiro-session      3          7,000        0.0%          0      0.00\n",
  "gemini   gemini-3-pro                             0              0        0.0%          0      0.00\n",
];
const FOOTER =
  "\nTOTAL  rows=6  messages=300  tokens=122,310,010  cache-read=98.9%  output=255,600  cost=$45.94\n" +
  "cost = API-list-price estimate from local logs; subscription/credit billing differs.\n";

const TOKSCALE = "-y tokscale@4.17.0 models --json -c";

beforeEach(() => {
  base = realpathSync(mkdtempSync(join(tmpdir(), "ws-usage-")));
  fakebin = join(base, "bin");
  mkdirSync(fakebin);
  argsLog = join(base, "npx-args");
  fixture = join(base, "fixture.json");
  writeFileSync(fixture, JSON.stringify(FIXTURE));
  fakeNpx('cat "$FIXTURE"');
});

afterEach(() => rmSync(base, { recursive: true, force: true }));

describe("ws usage", () => {
  test("default: last week, all four CLIs, by client+model, sorted by cost", () => {
    const r = ws(["usage"]);
    assert.deepEqual([r.code, r.err], [0, ""]);
    assert.equal(r.out, HEADER + ROWS.join("") + FOOTER);
    assert.equal(npxArgs(), `${TOKSCALE} claude,codex,gemini,kiro --group-by client,model --week`);
  });

  test("options are passed through in order; --top limits rows, negative as a Python slice", () => {
    let r = ws(["usage", "--today", "--by-session", "--clients", "kiro", "--top", "2"]);
    assert.equal(r.code, 0, r.err);
    assert.equal(r.out, HEADER + ROWS.slice(0, 2).join("") + FOOTER);
    assert.equal(npxArgs(), `${TOKSCALE} kiro --group-by client,session,model --today`);
    r = ws(["usage", "--since", "2026-09-01", "--until", "2026-09-30", "--week", "--top", "-4"]);
    assert.equal(r.out, HEADER + ROWS.slice(0, 2).join("") + FOOTER);
    assert.equal(npxArgs(), `${TOKSCALE} claude,codex,gemini,kiro --group-by client,model --since 2026-09-01 --until 2026-09-30 --week`);
    r = ws(["usage", "--top", " +0 "]);
    assert.equal(r.out, HEADER + FOOTER);
  });

  test("TOKSCALE_VERSION overrides the pinned version", () => {
    assert.equal(ws(["usage"], { TOKSCALE_VERSION: "9.9.9" }).code, 0);
    assert.match(npxArgs() ?? "", /^-y tokscale@9\.9\.9 models /);
  });

  test("no npx: message and exit 1", () => {
    rmSync(join(fakebin, "npx"));
    const r = ws(["usage"]);
    assert.deepEqual([r.code, r.out, r.err], [1, "", "npx not found (install Node.js)\n"]);
  });

  test("tokscale failure: its stderr is shown and its exit code returned", () => {
    fakeNpx("echo 'tokscale: boom' >&2; exit 3");
    const r = ws(["usage"]);
    assert.deepEqual([r.code, r.out, r.err], [3, "", "tokscale: boom\n"]);
  });

  test("bad arguments print usage to stderr and exit 2 without running tokscale", () => {
    for (const args of [["--bogus"], ["--top"], ["--since"], ["--clients"], ["extra"]]) {
      const r = ws(["usage", ...args]);
      assert.equal(r.code, 2, args.join(" "));
      assert.equal(r.out, "");
      assert.match(r.err, /^Usage: .*usage \[--today\|--week\|--month\|--since YYYY-MM-DD \[--until YYYY-MM-DD\]\]\n/);
    }
    assert.equal(npxArgs(), null);
  });

  test("an invalid --top fails with exit 1 and no report", () => {
    const r = ws(["usage", "--top", "ten"]);
    assert.equal(r.code, 1);
    assert.equal(r.out, "");
  });

  test("--help prints usage to stdout and exits 0 (deliberate: the script exited 2)", () => {
    const r = ws(["usage", "--help"]);
    assert.equal(r.code, 0);
    assert.match(r.out, /^Usage: ws usage /);
    assert.equal(npxArgs(), null);
  });
});

describe("ws session-report", () => {
  const REPORT =
    "4 sessions, $46 estimated (API list price)\n" +
    " msgs/session sessions  median tok/msg cost share\n" +
    "         1-20        2          30,607       5.4%\n" +
    "        21-50        1           1,733       9.8%\n" +
    "      201-400        1         485,000      84.8%\n" +
    "pearson r(log messages, tokens per message) = 0.85\n";

  test("report: sessions merged by id, empty sessions dropped, buckets and correlation", () => {
    const r = ws(["session-report"]);
    assert.deepEqual([r.code, r.err], [0, ""]);
    assert.equal(r.out, REPORT);
    assert.equal(npxArgs(), `${TOKSCALE} claude --group-by client,session,model --month`);
  });

  test("--since and --clients, also as unique prefixes and --opt=value; empty --since means --month", () => {
    ws(["session-report", "--since", "2026-09-01", "--clients", "claude,kiro"]);
    assert.equal(npxArgs(), `${TOKSCALE} claude,kiro --group-by client,session,model --since 2026-09-01`);
    const r = ws(["session-report", "--si=2026-08-01", "--c", "kiro"]);
    assert.equal(r.out, REPORT);
    assert.equal(npxArgs(), `${TOKSCALE} kiro --group-by client,session,model --since 2026-08-01`);
    ws(["session-report", "--since", ""]);
    assert.equal(npxArgs(), `${TOKSCALE} claude --group-by client,session,model --month`);
  });

  test("fewer than 3 sessions: exit 1", () => {
    writeFileSync(fixture, JSON.stringify({ ...FIXTURE, entries: FIXTURE.entries.slice(0, 2) }));
    const r = ws(["session-report"]);
    assert.deepEqual([r.code, r.out, r.err], [1, "", "not enough sessions in range\n"]);
  });

  test("tokscale failure or missing npx: exit 1, no report", () => {
    fakeNpx("echo boom >&2; exit 3");
    let r = ws(["session-report"]);
    assert.deepEqual([r.code, r.out], [1, ""]);
    assert.notEqual(r.err, "");
    rmSync(join(fakebin, "npx"));
    r = ws(["session-report"]);
    assert.deepEqual([r.code, r.out], [1, ""]);
    assert.notEqual(r.err, "");
  });

  test("argument errors exit 2 with argparse messages; --help exits 0", () => {
    const cases: [string[], string][] = [
      [["--bogus"], "unrecognized arguments: --bogus"],
      [["extra"], "unrecognized arguments: extra"],
      [["--since"], "argument --since: expected one argument"],
      [["--clients", "--since", "x"], "argument --clients: expected one argument"],
    ];
    for (const [args, msg] of cases) {
      const r = ws(["session-report", ...args]);
      assert.equal(r.code, 2, args.join(" "));
      assert.equal(r.out, "");
      assert.match(r.err, /^usage: .*session-report \[-h\] \[--since SINCE\] \[--clients CLIENTS\]\n/);
      assert.ok(r.err.endsWith(`session-report: error: ${msg}\n`), r.err);
    }
    assert.equal(npxArgs(), null);
    const h = ws(["session-report", "--help"]);
    assert.equal(h.code, 0);
    assert.match(h.out, /^usage: .*session-report \[-h\]/);
    assert.match(h.out, /\n  --since SINCE\n  --clients CLIENTS\n$/);
  });
});
