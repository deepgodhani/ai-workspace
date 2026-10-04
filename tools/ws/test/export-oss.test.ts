// Black-box tests for `ws export-oss` (port of bin/export-oss, Python).
// This file is itself exported and scanned, so private markers, secrets, and personal
// emails in fixtures are assembled at run time and never appear literally here.
import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import {
  chmodSync,
  existsSync,
  lstatSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  readlinkSync,
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
let dest: string;

const MD_BEGIN = "<!-- private:" + "begin -->";
const MD_END = "<!-- private:" + "end -->";
const HASH_BEGIN = "# private:" + "begin";
const HASH_END = "# private:" + "end";

function exportOss(args: string[] = [], env: Record<string, string> = {}) {
  const e: Record<string, string> = { ...(process.env as Record<string, string>), WS_ROOT: ws, HOME: join(base, "home"), ...env };
  delete e.ORCH_WORKSPACE;
  const r = spawnSync(WS_BIN, ["export-oss", ...args], { cwd: base, encoding: "utf8", env: e });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

function put(rel: string, content: string | Buffer = "", mode = 0o644) {
  const p = join(ws, rel);
  mkdirSync(resolve(p, ".."), { recursive: true });
  writeFileSync(p, content);
  chmodSync(p, mode);
}

// Files and links under `dir` (not .git), with link targets.
function listing(dir: string): string[] {
  const out: string[] = [];
  const walk = (d: string, rel: string) => {
    for (const n of readdirSync(d).sort()) {
      if (n === ".git") continue;
      const p = join(d, n);
      const r = rel ? `${rel}/${n}` : n;
      const s = lstatSync(p);
      if (s.isSymbolicLink()) out.push(`${r} -> ${readlinkSync(p)}`);
      else if (s.isDirectory()) walk(p, r);
      else out.push(r);
    }
  };
  walk(dir, "");
  return out;
}

function regularBytes(dir: string): number {
  let n = 0;
  const walk = (d: string) => {
    for (const name of readdirSync(d)) {
      if (name === ".git") continue;
      const s = lstatSync(join(d, name));
      if (s.isDirectory()) walk(join(d, name));
      else if (s.isFile()) n += s.size;
    }
  };
  walk(dir);
  return n;
}

// Python's f"{x:.0f}": round half to even.
function kib(bytes: number): string {
  const x = bytes / 1024;
  const f = Math.floor(x);
  const d = x - f;
  return String(d > 0.5 || (d === 0.5 && f % 2 === 1) ? f + 1 : f);
}

const mode = (rel: string) => (lstatSync(join(dest, rel)).mode & 0o777).toString(8);
const read = (rel: string) => readFileSync(join(dest, rel), "utf8");

const MANIFEST = [
  "# Paths to export",
  "AGENTS.md",
  "",
  "CLAUDE.md",
  "_shared",
  "bin/tool",
  "docs",
  "agents/demo",
  "tools/ws",
  "sub",
].join("\n");

const EXPECTED = [
  ".gitignore",
  ".kiro/skills/alpha -> ../../_shared/skills/alpha",
  ".kiro/skills/workspace-coach -> ../../_shared/skills/workspace-coach",
  "AGENTS.md",
  "CLAUDE.md",
  "LICENSE",
  "README.md",
  "_shared/oss/LICENSE",
  "_shared/oss/README.md",
  "_shared/oss/gitignore",
  "_shared/oss/knowledge-README.md",
  "_shared/oss/manifest.txt",
  "_shared/oss/projects-README.md",
  "_shared/oss/temp-sessions-README.md",
  "_shared/skills/alpha/SKILL.md",
  "_shared/skills/beta/notes.md",
  "_shared/skills/workspace-coach/SKILL.md",
  "agents/demo/AGENTS.md",
  "agents/demo/cases/_template/case.md",
  "bin/tool",
  "docs/crlf.md",
  "docs/img.png",
  "docs/latin1.md",
  "docs/link.md -> crlf.md",
  "knowledge/README.md",
  "knowledge/daily/.gitkeep",
  "knowledge/inbox/.gitkeep",
  "knowledge/indexes/.gitkeep",
  "projects/README.md",
  "research/topics/.gitkeep",
  "sub/.env.example",
  "temp-sessions/README.md",
  "tools/database-port-registry.yaml",
  "tools/ws/src/a.cpp",
  "worktrees/.gitkeep",
];
const STAGED = 21; // EXPECTED minus generated scaffolding

const REGISTRY =
  "schema_version: '1.0'\nscope: local_database_endpoints\npolicy:\n  bind_host: 127.0.0.1\n  notes:\n" +
  "  - Entries record ownership and purpose, not whether a process is currently live.\n" +
  "  - Do not reuse a port unless owner, case, role, and destination all match.\n" +
  "  - Store no credentials or connection strings here.\nassignments: []\n";

beforeEach(() => {
  base = realpathSync(mkdtempSync(join(tmpdir(), "ws-export-")));
  ws = join(base, "ws");
  dest = join(base, "dest");
  mkdirSync(join(base, "home"));
  put("_shared/oss/manifest.txt", MANIFEST + "\n");
  for (const f of ["README.md", "gitignore", "LICENSE", "knowledge-README.md", "projects-README.md", "temp-sessions-README.md"])
    put(`_shared/oss/${f}`, `generated ${f}\n`);
  put(
    "AGENTS.md",
    `# Root\nkeep 1\n  ${MD_BEGIN}  \nprivate line\n${MD_END}   \nkeep 2\ninline ${MD_BEGIN}x${MD_END} tail\n`,
    0o640,
  );
  put("CLAUDE.md", "@AGENTS.md\n");
  put("_shared/skills/alpha/SKILL.md", "# alpha\n");
  put("_shared/skills/beta/notes.md", "no skill file\n");
  put("_shared/skills/workspace-coach/SKILL.md", "# coach\n");
  put("_shared/skills/workspace-coach/references/verification-agent-workflow.md", "private workflow\n");
  put("bin/tool", `#!/bin/sh\necho a\n  ${HASH_BEGIN}\necho private\n\t${HASH_END}  \necho b\n`, 0o755);
  put("docs/crlf.md", "one\r\ntwo\rthree\n");
  put("docs/latin1.md", Buffer.from([0x63, 0x61, 0x66, 0xe9, 0x0d, 0x0a])); // not UTF-8: copied as is
  put("docs/img.png", Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00]), 0o640);
  symlinkSync("crlf.md", join(ws, "docs/link.md"));
  for (const junk of ["docs/a.pyc", "docs/x.bak", "docs/y.orig", "docs/.DS_Store", "docs/node_modules/m.js", "docs/__pycache__/c.py"])
    put(junk, "junk\n");
  put("agents/demo/AGENTS.md", "# demo\n");
  put("agents/demo/cases/_template/case.md", "template\n");
  put("agents/demo/cases/real-case/case.md", "real case\n");
  put("tools/ws/build/ws", "binary\n");
  put("tools/ws/src/a.cpp", `// ${MD_BEGIN} stays in C++ ${MD_END}\n`);
  put("sub/.env", "TOKEN=x\n");
  put("sub/.env.local", "TOKEN=y\n");
  put("sub/.env.example", "TOKEN=\n");
});

afterEach(() => rmSync(base, { recursive: true, force: true }));

describe("ws export-oss", () => {
  test("--check stages and scans without touching DEST", () => {
    const r = exportOss([dest, "--check"]);
    assert.equal(r.err, "");
    assert.match(r.out, new RegExp(`^staged ${STAGED} framework files \\(\\+ generated scaffolding\\), \\d+ KiB; denylist terms: 0\nscan clean\n$`));
    assert.equal(r.code, 0);
    assert.ok(!existsSync(dest));
  });

  test("export: manifest copy, skip rules, private stripping, newlines, modes, scaffolding, git init", () => {
    const r = exportOss([dest]);
    assert.equal(r.code, 0, r.out + r.err);
    assert.equal(r.err, "");
    assert.deepEqual(listing(dest), EXPECTED);
    const changed = execFileSync("git", ["-C", dest, "status", "--short"], { encoding: "utf8" }).split("\n").filter(Boolean).length;
    assert.equal(
      r.out,
      `staged ${STAGED} framework files (+ generated scaffolding), ${kib(regularBytes(dest))} KiB; denylist terms: 0\n` +
        `scan clean\nexported to ${dest}; ${changed} path(s) changed vs last commit (nothing committed)\n`,
    );
    assert.ok(existsSync(join(dest, ".git/HEAD")), "git init");
    assert.equal(read("AGENTS.md"), "# Root\nkeep 1\nkeep 2\ninlinetail\n");
    assert.equal(read("bin/tool"), "#!/bin/sh\necho a\necho b\n");
    assert.equal(read("docs/crlf.md"), "one\ntwo\nthree\n");
    assert.deepEqual([...readFileSync(join(dest, "docs/latin1.md"))], [0x63, 0x61, 0x66, 0xe9, 0x0d, 0x0a]);
    assert.equal(read("tools/ws/src/a.cpp"), `// ${MD_BEGIN} stays in C++ ${MD_END}\n`);
    assert.equal(read(".gitignore"), "generated gitignore\n");
    assert.equal(read("tools/database-port-registry.yaml"), REGISTRY);
    assert.equal(read("research/topics/.gitkeep"), "");
    assert.deepEqual([mode("AGENTS.md"), mode("bin/tool"), mode("docs/img.png")], ["640", "755", "640"]);
  });

  test("the export's own templates are exported verbatim, so a clone can export again (deliberate: the script skipped _shared/oss)", () => {
    put("_shared/oss/README.md", `quotes ${MD_BEGIN} and ${MD_END} literally\n`);
    assert.equal(exportOss([dest]).code, 0);
    assert.equal(read("_shared/oss/README.md"), `quotes ${MD_BEGIN} and ${MD_END} literally\n`);
    assert.equal(read("README.md"), `quotes ${MD_BEGIN} and ${MD_END} literally\n`);
    const again = join(base, "again");
    const r = spawnSync(WS_BIN, ["export-oss", again], {
      cwd: base,
      encoding: "utf8",
      env: { ...(process.env as Record<string, string>), WS_ROOT: dest, HOME: join(base, "home") },
    });
    assert.equal(r.status, 0, r.stdout + r.stderr);
    assert.deepEqual(listing(again), listing(dest));
  });

  test("re-export syncs: extras deleted, changes restored, nested .git kept", () => {
    assert.equal(exportOss([dest]).code, 0);
    writeFileSync(join(dest, "extra.txt"), "junk\n");
    mkdirSync(join(dest, "gone/.git"), { recursive: true });
    writeFileSync(join(dest, "gone/.git/HEAD"), "kept\n");
    writeFileSync(join(dest, "gone/file"), "deleted\n");
    writeFileSync(join(dest, "AGENTS.md"), "edited\n");
    rmSync(join(dest, "docs"), { recursive: true });
    writeFileSync(join(dest, "docs"), "now a file\n");
    chmodSync(join(dest, "CLAUDE.md"), 0o600);
    rmSync(join(dest, "README.md"));
    symlinkSync("/etc", join(dest, "README.md"));
    const r = exportOss([dest]);
    assert.equal(r.code, 0, r.out + r.err);
    assert.ok(r.err === "" || /gone: not empty, cannot delete\n$/.test(r.err), r.err); // rsync warned only sometimes
    assert.deepEqual(listing(dest), EXPECTED);
    assert.equal(read("AGENTS.md"), "# Root\nkeep 1\nkeep 2\ninlinetail\n");
    assert.equal(read("README.md"), "generated README.md\n");
    assert.equal(mode("CLAUDE.md"), "644");
    assert.equal(readFileSync(join(dest, "gone/.git/HEAD"), "utf8"), "kept\n");
    assert.match(r.out, /\nexported to .*; \d+ path\(s\) changed vs last commit \(nothing committed\)\n$/);
  });

  test("findings block the export: secrets, home path, denylist, emails, escaping links", () => {
    const home = join(base, "home");
    const lines = [
      "key " + "sk-" + "ant-" + "a".repeat(20), // 1
      "fc-" + "b".repeat(20) + " and x" + "fc-" + "b".repeat(20), // 2: one hit per rule per line
      "AKIA" + "ABCDEFGHIJKLMNOP", // 3
      "AKIA" + "ABCDEFGHIJKLMNOPQ", // 4: 17 characters, no word boundary
      "ghp_" + "c".repeat(30) + " xoxb-" + "d".repeat(10), // 5: two rules
      "-----BEGIN " + "RSA PRIVATE KEY-----", // 6
      "host ec2-1-2.compute" + "-1.amazon" + "aws.com", // 7
      `path ${home}/notes`, // 8
      `path ${home}s/notes`, // 9: no word boundary after the home path
      "ACME corp builds", // 10: denylist, case-insensitive phrase
      "acme corpx and xacme corp", // 11: no hit (letters on either side)
      "zeta_1", // 12: "_" is not in [A-Za-z0-9]
      "mail " + "dev" + "@" + "corp.io" + ", ok@example.com, " + "b" + "@" + "corp.io", // 13: two hits
      "Ok@Users.Noreply.GitHub.com", // 14: allowed domain, any case
      "form\ffeed counts as a line break", // 15 and 16
      "ZETA", // 17
    ];
    put("docs/bad.md", lines.join("\n") + "\n");
    symlinkSync("/etc/hosts", join(ws, "docs/abs-link"));
    symlinkSync("../../outside", join(ws, "docs/rel-link"));
    symlinkSync("../AGENTS.md", join(ws, "docs/inside-link"));
    writeFileSync(join(base, "deny.txt"), "# comment\n\n  Acme Corp  \nzeta\nzeta\n");
    const r = exportOss([dest, "--denylist", join(base, "deny.txt")]);
    assert.equal(r.err, "");
    assert.equal(r.code, 1);
    const hits = [
      "docs/abs-link: symlink leaves the repo -> /etc/hosts",
      "docs/bad.md:1: anthropic/openai key",
      "docs/bad.md:2: firecrawl key",
      "docs/bad.md:3: aws access key",
      "docs/bad.md:5: github token",
      "docs/bad.md:5: slack token",
      "docs/bad.md:6: private key",
      "docs/bad.md:7: aws hostname",
      "docs/bad.md:8: home path",
      "docs/bad.md:10: denylist 'Acme Corp'",
      "docs/bad.md:12: denylist 'zeta'",
      "docs/bad.md:13: email address",
      "docs/bad.md:13: email address",
      "docs/bad.md:17: denylist 'zeta'",
      "docs/rel-link: symlink leaves the repo -> ../../outside",
    ];
    assert.equal(
      r.out,
      `staged ${STAGED + 4} framework files (+ generated scaffolding), ${r.out.match(/, (\d+) KiB/)?.[1]} KiB; denylist terms: 3\n` +
        `BLOCKED: ${hits.length} finding(s); nothing written:\n` +
        hits.map((h) => `  ${h}\n`).join(""),
    );
    assert.ok(!existsSync(dest), "nothing written");
  });

  test("at most 50 findings are listed", () => {
    put("docs/many.md", Array.from({ length: 60 }, () => "AKIA" + "ABCDEFGHIJKLMNOP").join("\n"));
    const r = exportOss([dest, "--check"]);
    assert.equal(r.code, 1);
    assert.match(r.out, /\nBLOCKED: 60 finding\(s\); nothing written:\n/);
    assert.equal(r.out.split("\n").filter((l) => l.startsWith("  docs/many.md:")).length, 50);
    assert.match(r.out, /  docs\/many\.md:50: aws access key\n$/);
  });

  test("the default denylist is <workspace>/.oss-denylist", () => {
    put(".oss-denylist", "keep 2\n");
    const r = exportOss(["--check"]);
    assert.equal(r.code, 1);
    assert.match(r.out, /denylist terms: 1\nBLOCKED: 1 finding\(s\); nothing written:\n  AGENTS\.md:3: denylist 'keep 2'\n$/);
  });

  test("errors: missing manifest path, unbalanced private markers", () => {
    put("_shared/oss/manifest.txt", MANIFEST + "\nnot-there\n");
    let r = exportOss([dest]);
    assert.deepEqual([r.code, r.out, r.err], [1, "", "error: manifest path missing: not-there\n"]);
    put("_shared/oss/manifest.txt", MANIFEST + "\n");
    put("docs/half.md", `${MD_BEGIN}\nno end\n`);
    r = exportOss([dest]);
    assert.deepEqual([r.code, r.out, r.err], [1, "", "error: unbalanced private markers in docs/half.md\n"]);
    assert.ok(!existsSync(dest));
  });

  test("arguments: --help, unknown option, extra positional, missing --denylist value", () => {
    const h = exportOss(["--help"]);
    assert.equal(h.code, 0);
    assert.match(h.out, /^usage: .*export-oss \[-h\] \[--check\] \[--denylist DENYLIST\] \[dest\]\n/);
    for (const [args, msg] of [
      [["--bogus"], "unrecognized arguments: --bogus"],
      [["a", "b"], "unrecognized arguments: b"],
      [["--denylist"], "argument --denylist: expected one argument"],
    ] as const) {
      const r = exportOss([...args]);
      assert.equal(r.code, 2, args.join(" "));
      assert.equal(r.out, "");
      assert.ok(r.err.endsWith(`export-oss: error: ${msg}\n`), r.err);
    }
    assert.ok(!existsSync(join(base, "a")));
  });
});
