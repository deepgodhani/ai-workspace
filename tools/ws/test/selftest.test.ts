// Black-box tests for `ws selftest` (port of bin/selftest).
// The workspace under test is a small fake: its bin/ tools and tools/ws/Makefile are shell
// stubs that log how they were called to $SELFTEST_PROBE.
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import {
  chmodSync,
  existsSync,
  lstatSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  realpathSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { afterEach, beforeEach, describe, test } from "node:test";

const ROOT = resolve(import.meta.dirname, "..");
const WS_BIN = process.env.WS_BIN ?? join(ROOT, "build", "ws");
let base: string;
let ws: string;
let tmp: string; // TMPDIR for the run, so leftovers are visible
let probe: string;

function selftest(args: string[] = [], extraEnv: Record<string, string> = {}) {
  const env: Record<string, string> = {
    ...(process.env as Record<string, string>),
    WS_ROOT: ws,
    TMPDIR: tmp,
    SELFTEST_PROBE: probe,
    ...extraEnv,
  };
  if (!("ORCH_WORKSPACE" in extraEnv)) delete env.ORCH_WORKSPACE;
  const r = spawnSync(WS_BIN, ["selftest", ...args], { cwd: tmpdir(), encoding: "utf8", env });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

function put(rel: string, content = "", mode = 0o644) {
  const p = join(ws, rel);
  mkdirSync(resolve(p, ".."), { recursive: true });
  writeFileSync(p, content);
  chmodSync(p, mode);
}

// A bin/ stub: logs "<name> <args> | <cwd> | <HOME>" and then runs `body`.
const stub = (rel: string, body = "") =>
  put(rel, `#!/bin/sh\necho "${rel} $* | $(pwd -P) | $HOME" >> "$SELFTEST_PROBE"\n${body}\n`, 0o755);

const probeLines = () => (existsSync(probe) ? readFileSync(probe, "utf8").trimEnd().split("\n") : []);

function snapshot(dir: string): string[] {
  const out: string[] = [];
  const walk = (d: string) => {
    for (const n of readdirSync(d).sort()) {
      const p = join(d, n);
      const s = lstatSync(p);
      out.push(`${p} ${s.mode} ${s.size} ${s.mtimeMs}`);
      if (s.isDirectory()) walk(p);
    }
  };
  walk(dir);
  // The old-script shim (WS_BIN) installs the script it runs at bin/selftest.
  return out.filter((l) => !l.startsWith(join(ws, "bin/selftest") + " "));
}

const NAMES = [
  "ws builds from source",
  "ws runs",
  "link-skills into Claude/Codex/Kiro",
  "Claude skill links resolve",
  "Codex skill links resolve",
  "Kiro skill links resolve",
  "new-project",
  "project has state files",
  "new-research",
  "research topic has scope",
  "new-agent-case",
  "agent case has target.yaml",
  "workspace-context resolves a target",
  "root contract under 4 KB",
  "CLAUDE.md imports AGENTS.md",
  "Kiro agent configs present",
  "workspace-doctor runs",
  "orch dry run builds a context packet",
];

beforeEach(() => {
  base = realpathSync(mkdtempSync(join(tmpdir(), "ws-selftest-")));
  ws = join(base, "ws");
  tmp = join(base, "tmp");
  probe = join(base, "probe.log");
  mkdirSync(tmp);
  put("AGENTS.md", "# root\n");
  put("CLAUDE.md", "@AGENTS.md\n");
  put(".kiro/agents/repo-auditor.md", "# auditor\n");
  put("bin/selftest"); // replaced by the old-script shim; keeps snapshots comparable
  put("tools/ws/Makefile", 'all:\n\t@echo "make | $$(cd ../.. && pwd -P) | $$HOME" >> "$$SELFTEST_PROBE"\n');
  stub("bin/ws");
  stub(
    "bin/link-skills",
    'for d in "$HOME/.claude/skills" "$HOME/.agents/skills" .kiro/skills; do mkdir -p "$d/workspace-coach" && : > "$d/workspace-coach/SKILL.md"; done',
  );
  stub("bin/new-project", 'mkdir -p "projects/$1/.ai" && : > "projects/$1/.ai/STATE.md"');
  stub("bin/new-research", 'mkdir -p "research/topics/$1" && : > "research/topics/$1/SCOPE.md"');
  stub("bin/new-agent-case", 'mkdir -p "agents/$1/cases/$2" && : > "agents/$1/cases/$2/target.yaml"');
  stub("bin/workspace-context");
  stub("bin/workspace-doctor", "echo noisy; echo noisy >&2");
  stub("bin/orch", 'echo "orch env WS_ROOT=${WS_ROOT-unset} ORCH_WORKSPACE=${ORCH_WORKSPACE-unset}" >> "$SELFTEST_PROBE"');
});

afterEach(() => rmSync(base, { recursive: true, force: true }));

describe("ws selftest", () => {
  test("all checks pass: runs every tool in the copy with an isolated HOME, then removes it", () => {
    const before = snapshot(ws);
    const r = selftest();
    assert.equal(r.err, "");
    assert.equal(r.out, NAMES.map((n) => `PASS  ${n}\n`).join("") + "\n18 passed, 0 failed\n");
    assert.equal(r.code, 0);
    const lines = probeLines();
    const where = lines[0].split(" | ").slice(1);
    const [cwd, home] = where;
    assert.match(cwd, /\/tmp\.[A-Za-z0-9]+\/ws$/); // under $TMPDIR (macOS mktemp used the user temp dir)
    assert.ok(!existsSync(resolve(cwd, "..")), "the copy is removed");
    const unprivate = (p: string) => p.replace(/^\/private\//, "/"); // macOS /var -> /private/var
    assert.equal(unprivate(home), unprivate(resolve(cwd, "../home")));
    assert.deepEqual(
      lines.slice(0, -1).map((l) => l.split(" | ")[0]), // the last line (orch's env) has its own test
      [
        "make",
        "bin/ws version",
        "bin/link-skills ",
        "bin/new-project selftest-app",
        "bin/new-research selftest-topic",
        "bin/new-agent-case repo-auditor selftest-case",
        "bin/workspace-context projects/selftest-app",
        "bin/workspace-doctor ",
        "bin/orch spawn projects/selftest-app --task selftest --dry-run",
      ],
    );
    for (const l of lines.slice(0, -1)) assert.deepEqual(l.split(" | ").slice(1), where, l);
    assert.deepEqual(readdirSync(tmp), [], "the copy is removed");
    assert.deepEqual(snapshot(ws), before, "the source workspace is untouched");
  });

  test("copies like rsync -a with the script's excludes", () => {
    const listing = join(base, "listing.txt");
    stub("bin/ws", `find . -print | LC_ALL=C sort > "${listing}"; stat -f '%N %Lp' bin/tool secret.txt 2>/dev/null >> "${listing}"; readlink link >> "${listing}"`);
    put("bin/tool", "", 0o750);
    put("secret.txt", "", 0o600);
    symlinkSync("AGENTS.md", join(ws, "link"));
    for (const rel of [
      ".git/HEAD",
      ".orch/registry.db",
      "projects/app/README.md",
      "research/README.md",
      "research/topics/t/SCOPE.md",
      "knowledge/k.md",
      "temp-sessions/s/x",
      "worktrees/w/x",
      "tools/ws/build/ws",
      "tools/ws/src/main.cpp",
      "tools/local-research/work/x",
      "tools/local-research/.venv/x",
      "tools/local-research/README.md",
      "_shared/templates/projects/x", // a bare name matches at any depth
      "_shared/sub/.git",
      "agents/a/tools/ws/build/x", // a path rule matches the trailing components
      "agents/a/research/topics2/x",
    ])
      put(rel);
    const r = selftest();
    assert.equal(r.code, 0, r.out + r.err);
    const got = readFileSync(listing, "utf8").trimEnd().split("\n");
    const want = [
      ".",
      "./.kiro",
      "./.kiro/agents",
      "./.kiro/agents/repo-auditor.md",
      "./AGENTS.md",
      "./CLAUDE.md",
      "./_shared",
      "./_shared/sub",
      "./_shared/templates",
      "./agents",
      "./agents/a",
      "./agents/a/research",
      "./agents/a/research/topics2",
      "./agents/a/research/topics2/x",
      "./agents/a/tools",
      "./agents/a/tools/ws",
      "./bin",
      ...["link-skills", "new-agent-case", "new-project", "new-research", "orch", "selftest", "tool", "workspace-context", "workspace-doctor", "ws"].map(
        (n) => `./bin/${n}`,
      ),
      "./knowledge",
      "./link",
      "./projects",
      "./research",
      "./research/README.md",
      "./research/topics",
      "./secret.txt",
      "./tools",
      "./tools/local-research",
      "./tools/local-research/README.md",
      "./tools/ws",
      "./tools/ws/Makefile",
      "./tools/ws/src",
      "./tools/ws/src/main.cpp",
      "bin/tool 750",
      "secret.txt 600",
      "AGENTS.md",
    ];
    assert.deepEqual(got, want);
  });

  test("failures are listed and counted in the exit code; noisy tools stay quiet", () => {
    put("tools/ws/Makefile", "all:\n\t@echo build-out; echo build-err >&2; exit 1\n");
    rmSync(join(ws, "bin/new-research"));
    put("CLAUDE.md", "no import\n");
    put("AGENTS.md", "x".repeat(4096));
    const r = selftest();
    const failed = ["ws builds from source", "new-research", "research topic has scope", "root contract under 4 KB", "CLAUDE.md imports AGENTS.md"];
    assert.equal(r.out, NAMES.map((n) => `${failed.includes(n) ? "FAIL" : "PASS"}  ${n}\n`).join("") + "\n13 passed, 5 failed\n");
    assert.equal(r.err, "");
    assert.equal(r.code, 5);
    assert.deepEqual(readdirSync(tmp), []);
  });

  test("the copy never resolves to the source workspace (deliberate: WS_ROOT/ORCH_WORKSPACE are cleared)", () => {
    const r = selftest([], { ORCH_WORKSPACE: ws });
    assert.equal(r.code, 0, r.out + r.err);
    assert.equal(probeLines().at(-1), "orch env WS_ROOT=unset ORCH_WORKSPACE=unset");
  });

  test("--help exits 0 without copying", () => {
    const r = selftest(["--help"]);
    assert.equal(r.code, 0);
    assert.match(r.out, /^Usage: ws selftest\n/);
    assert.deepEqual(readdirSync(tmp), []);
    assert.ok(!existsSync(probe));
  });
});
