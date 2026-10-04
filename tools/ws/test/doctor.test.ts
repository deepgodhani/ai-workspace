// Black-box tests for `ws doctor` (port of bin/workspace-doctor).
// PATH holds only a fake tool folder, so the report does not depend on this machine.
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { chmodSync, lstatSync, mkdirSync, mkdtempSync, readdirSync, realpathSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { afterEach, beforeEach, describe, test } from "node:test";

const ROOT = resolve(import.meta.dirname, "..");
const WS_BIN = process.env.WS_BIN ?? join(ROOT, "build", "ws");
let base: string;
let ws: string;
let home: string;
let fakebin: string;

function doctor(args: string[] = [], opts: { home?: boolean } = {}) {
  const env: Record<string, string> = { PATH: fakebin, WS_ROOT: ws };
  if (opts.home !== false) env.HOME = home;
  const r = spawnSync(WS_BIN, ["doctor", ...args], { cwd: tmpdir(), encoding: "utf8", env });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

function put(rel: string, content = "", mode = 0o644) {
  const p = join(ws, rel);
  mkdirSync(resolve(p, ".."), { recursive: true });
  writeFileSync(p, content);
  chmodSync(p, mode);
}

function tool(name: string, body: string, mode = 0o755) {
  writeFileSync(join(fakebin, name), `#!/bin/sh\n${body}\n`);
  chmodSync(join(fakebin, name), mode);
}

// Every path under `dir` with its type, mode, size and mtime.
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
  // The old-script shim (WS_BIN) installs the script it runs at bin/workspace-doctor.
  return out.filter((l) => !l.startsWith(join(ws, "bin/workspace-doctor") + " "));
}

const PRIVATE = ["--agent-readme", "verifier", "--check-exec", "bin/private-tool"];

beforeEach(() => {
  base = realpathSync(mkdtempSync(join(tmpdir(), "ws-doctor-")));
  ws = join(base, "ws");
  home = join(base, "home");
  fakebin = join(base, "fakebin");
  mkdirSync(ws);
  mkdirSync(home);
  mkdirSync(fakebin);
  // The bash original needs these; they are not in the doctor's tool list.
  for (const t of ["awk", "sort", "uniq", "dirname", "basename"]) {
    const real = spawnSync("/bin/sh", ["-c", `command -v ${t}`], { encoding: "utf8" }).stdout.trim();
    symlinkSync(real, join(fakebin, t));
  }
});

afterEach(() => rmSync(base, { recursive: true, force: true }));

describe("ws doctor", () => {
  test("full report: tools, structure, agents, duplicate ports, skills, child output", () => {
    tool("git", "exit 0");
    tool("codex", 'echo "codex-mcp $*"; echo codex-err >&2');
    tool("claude", 'echo "claude-mcp $*"; echo claude-err >&2; exit 4');
    for (const d of ["_shared", "bin", "tools"]) mkdirSync(join(ws, d), { recursive: true });
    put("agents/a1/AGENTS.md");
    mkdirSync(join(ws, "agents/b2"));
    mkdirSync(join(ws, "agents/.hidden"));
    put("agents/README.md");
    symlinkSync("../_shared", join(ws, "agents/zlink")); // a link to a folder counts as an agent
    put("agents/verifier/README.md");
    put("bin/new-agent-case", "#!/bin/sh\n", 0o755);
    put("bin/workspace-context", "#!/bin/sh\n", 0o644); // not executable
    put("bin/workspace-doctor"); // so the old-script shim's copy adds no folder entry
    put("_shared/skills/handoff/SKILL.md");
    put("_shared/skills/local-web-research/SKILL.md");
    put(
      "tools/database-port-registry.yaml",
      "assignments:\n  - port: 27018\n  -   port: 27017 # c\n\t-\tport: 27017\n  - port: 5432\n  -port: 9999\n  - port: 5432\n",
    );
    put("tools/local-research/bin/status", "#!/bin/sh\necho 'status line'\necho 'status err' >&2\nexit 3\n", 0o755);
    mkdirSync(join(home, ".claude/skills/local-web-research"), { recursive: true });
    writeFileSync(join(home, ".claude/skills/local-web-research/SKILL.md"), "");

    const before = [...snapshot(ws), ...snapshot(home)];
    const r = doctor(PRIVATE);
    assert.equal(r.code, 0, r.err);
    assert.equal(r.err, "status err\n", "only the status script's stderr is shown");
    const pad = (s: string) => s.padEnd(12);
    assert.equal(
      r.out,
      [
        `Workspace: ${ws}`,
        "",
        `ok      ${pad("git")} ${fakebin}/git`,
        `missing ${pad("python3")}`,
        `missing ${pad("node")}`,
        `missing ${pad("npx")}`,
        `ok      ${pad("codex")} ${fakebin}/codex`,
        `ok      ${pad("claude")} ${fakebin}/claude`,
        `missing ${pad("gemini")}`,
        `missing ${pad("uv")}`,
        `missing ${pad("docker")}`,
        `missing ${pad("podman")}`,
        "",
        "Structure:",
        "ok      _shared/",
        "ok      agents/",
        "ok      bin/",
        "missing knowledge/",
        "missing research/",
        "missing projects/",
        "ok      tools/",
        "missing worktrees/",
        "",
        "Reusable agents:",
        "ok      agents/a1/AGENTS.md",
        "missing agents/b2/AGENTS.md",
        "missing agents/verifier/AGENTS.md",
        "missing agents/zlink/AGENTS.md",
        "ok      bin/new-agent-case",
        "ok      agents/verifier/",
        "missing or non-executable bin/private-tool",
        "ok      tools/database-port-registry.yaml",
        "warning duplicate registered database port(s): 27017",
        "5432",
        "",
        "Workspace lifecycle skills:",
        "missing _shared/skills/workspace-coach/SKILL.md",
        "ok      _shared/skills/handoff/SKILL.md",
        "missing _shared/skills/resume-work/SKILL.md",
        "missing _shared/skills/daily-summary/SKILL.md",
        "missing _shared/skills/temp-session/SKILL.md",
        "missing or non-executable bin/workspace-context",
        "missing or non-executable bin/init-ai-workspace.sh",
        "",
        "Local research:",
        "status line",
        `ok      ${ws}/_shared/skills/local-web-research/SKILL.md`,
        `missing ${home}/.agents/skills/local-web-research/SKILL.md`,
        `ok      ${home}/.claude/skills/local-web-research/SKILL.md`,
        "",
        "MCP status:",
        "Firecrawl is optional; local-web-research is the default.",
        "codex-mcp mcp list",
        "claude-mcp mcp list",
        "claude: unable to list MCP servers",
        "",
      ].join("\n"),
    );
    assert.deepEqual([...snapshot(ws), ...snapshot(home)], before, "doctor changes nothing");
  });

  test("empty workspace: everything missing, CLIs not installed, still exit 0", () => {
    const r = doctor(PRIVATE);
    assert.equal(r.code, 0, r.err);
    assert.equal(r.err, "");
    assert.match(
      r.out,
      /\nReusable agents:\nmissing or non-executable bin\/new-agent-case\nmissing agents\/verifier\/README\.md\nmissing or non-executable bin\/private-tool\nmissing tools\/database-port-registry\.yaml\n/,
    );
    assert.match(r.out, /\nLocal research:\nmissing tools\/local-research\/bin\/status\nmissing /);
    assert.ok(r.out.endsWith("default.\ncodex: not installed\nclaude: not installed\n"), r.out);
  });

  test("unique ports; a value glued to 'port:' is read as empty and empties do not warn", () => {
    put("tools/database-port-registry.yaml", "- port: 1\n- port: 2\n  - port:3\n  - port:4\n# - port: 1\n");
    const r = doctor();
    assert.equal(r.code, 0, r.err);
    assert.match(r.out, /^ok      tools\/database-port-registry\.yaml\nok      database port assignments are unique$/m);
  });

  test("HOME unset: stops after local research with exit 1", () => {
    const r = doctor([], { home: false });
    assert.equal(r.code, 1);
    assert.ok(r.out.endsWith("\nLocal research:\nmissing tools/local-research/bin/status\n"), r.out);
    assert.match(r.err, /HOME/);
  });

  test("a non-executable file on PATH is missing (deliberate: bash `command -v` reported it ok)", () => {
    tool("uv", "exit 0", 0o644);
    tool("codex", "exit 0", 0o644);
    const r = doctor();
    assert.equal(r.code, 0, r.err);
    assert.match(r.out, /^missing uv {10}$/m);
    assert.match(r.out, /^codex: not installed$/m);
  });

  test("private checks appear only when passed as flags (deliberate: they live in the bin/ wrapper)", () => {
    const r = doctor();
    assert.equal(r.code, 0, r.err);
    assert.doesNotMatch(r.out, /verifier|private-tool/);
    const p = doctor(["--check-exec", "bin/x", "--agent-readme", "v"]);
    assert.match(p.out, /^missing or non-executable bin\/new-agent-case\nmissing or non-executable bin\/x\nmissing agents\/v\/README\.md\n/m);
  });

  test("--help exits 0 without checking", () => {
    const r = doctor(["--help"]);
    assert.equal(r.code, 0);
    assert.match(r.out, /^Usage: ws doctor/);
    assert.doesNotMatch(r.out, /Workspace:/);
  });
});
