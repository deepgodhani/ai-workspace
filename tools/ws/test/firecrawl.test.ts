// Black-box tests for `ws firecrawl configure|mcp` (ports of bin/configure-firecrawl and
// bin/firecrawl-mcp-wrapper). Fake HOME and fake codex/claude/npx only; the real MCP
// configuration is never touched. Keys here are made up and too short to match the
// export scanner.
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { chmodSync, existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, statSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { afterEach, beforeEach, describe, test } from "node:test";

const ROOT = resolve(import.meta.dirname, "..");
const WS_BIN = process.env.WS_BIN ?? join(ROOT, "build", "ws");
let base: string;
let ws: string;
let home: string;
let fakebin: string;

function fc(sub: string, input = "", env: Record<string, string> = {}) {
  const e: Record<string, string> = { PATH: fakebin, HOME: home, WS_ROOT: ws, ...env };
  const r = spawnSync(WS_BIN, ["firecrawl", sub], { cwd: base, encoding: "utf8", env: e, input });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

function tool(name: string, body: string) {
  writeFileSync(join(fakebin, name), `#!/bin/sh\n${body}\n`);
  chmodSync(join(fakebin, name), 0o755);
}

const secretDir = () => join(home, ".config/ai-workspace");
const secretFile = () => join(secretDir(), "firecrawl.env");
const mode = (p: string) => (statSync(p).mode & 0o777).toString(8);
const seed = (text: string) => {
  mkdirSync(secretDir(), { recursive: true });
  writeFileSync(secretFile(), text);
};

beforeEach(() => {
  base = realpathSync(mkdtempSync(join(tmpdir(), "ws-firecrawl-")));
  ws = join(base, "ws");
  home = join(base, "home");
  fakebin = join(base, "fakebin");
  for (const d of [join(ws, "_shared"), join(ws, "bin"), home, fakebin]) mkdirSync(d, { recursive: true });
  writeFileSync(join(ws, "AGENTS.md"), "# root\n");
  // The bash originals need these; ws does not.
  for (const t of ["dirname", "mkdir", "chmod", "bash"]) {
    const real = spawnSync("/bin/sh", ["-c", `command -v ${t}`], { encoding: "utf8" }).stdout.trim();
    symlinkSync(real, join(fakebin, t));
  }
  tool("codex", 'echo "codex $*"');
  tool("claude", 'echo "claude $*"');
  tool("npx", 'echo "npx $* KEY=[$FIRECRAWL_API_KEY] URL=[${FIRECRAWL_API_URL-unset}]"; read x; echo "stdin=[$x]"');
});

afterEach(() => rmSync(base, { recursive: true, force: true }));

describe("ws firecrawl configure", () => {
  test("key from stdin: trimmed, saved as a shell line with mode 600, both clients configured", () => {
    const r = fc("configure", "  ab 1$x  \nrest\n");
    assert.equal(r.err, "");
    assert.equal(r.code, 0);
    const wrapper = join(ws, "bin/firecrawl-mcp-wrapper");
    assert.equal(
      r.out,
      "\n" +
        `Credential saved locally with mode 600: ${secretFile()}\n` +
        "Configuring Firecrawl MCP for Codex...\n" +
        `codex mcp add firecrawl -- ${wrapper}\n` +
        "Configuring Firecrawl MCP for Claude Code...\n" +
        `claude mcp add firecrawl -- ${wrapper}\n` +
        "Done. Restart each CLI and use /mcp to verify Firecrawl.\n",
    );
    assert.equal(readFileSync(secretFile(), "utf8"), "export FIRECRAWL_API_KEY=ab\\ 1\\$x\n");
    assert.equal(mode(secretDir()), "700");
    assert.equal(mode(secretFile()), "600");
  });

  test("key from FIRECRAWL_API_KEY, XDG_CONFIG_HOME respected, bash %q quoting", () => {
    const xdg = join(base, "xdg");
    const r = fc("configure", "", { FIRECRAWL_API_KEY: "#it's,a(b)", XDG_CONFIG_HOME: xdg });
    assert.equal(r.code, 0, r.err);
    assert.ok(!r.out.startsWith("\n"), "no newline when the key is not typed");
    assert.equal(readFileSync(join(xdg, "ai-workspace/firecrawl.env"), "utf8"), "export FIRECRAWL_API_KEY=\\#it\\'s\\,a\\(b\\)\n");
    assert.ok(!existsSync(secretFile()));
  });

  test("a control character uses $'…' quoting", () => {
    assert.equal(fc("configure", "a\tb\n").code, 0);
    assert.equal(readFileSync(secretFile(), "utf8"), "export FIRECRAWL_API_KEY=$'a\\tb'\n");
  });

  test("an existing wider file is narrowed to 600", () => {
    seed("old\n");
    chmodSync(secretFile(), 0o644);
    assert.equal(fc("configure", "k\n").code, 0);
    assert.equal(mode(secretFile()), "600");
  });

  test("one client failing still succeeds; a missing client is skipped", () => {
    rmSync(join(fakebin, "codex"));
    tool("claude", "echo claude-err >&2; exit 5");
    let r = fc("configure", "k\n");
    assert.equal(r.code, 1);
    assert.match(r.out, /\nskip Codex: command not installed\nConfiguring Firecrawl MCP for Claude Code\.\.\.\n$/);
    assert.equal(
      r.err,
      "claude-err\nClaude configuration failed. It may already have a server named firecrawl.\n" +
        "No clients were configured. The credential file was created but no MCP entry was added.\n",
    );
    assert.ok(existsSync(secretFile()));
    tool("codex", 'echo "codex $*"');
    r = fc("configure", "k\n");
    assert.equal(r.code, 0);
    assert.match(r.out, /Done\. Restart each CLI/);
  });

  test("errors: no npx, no client, empty key (2), EOF before a newline (1), all before writing", () => {
    rmSync(join(fakebin, "npx"));
    assert.deepEqual(Object.values(fc("configure", "k\n")), [1, "", "npx is required for the local Firecrawl MCP server.\n"]);
    tool("npx", "exit 0");
    rmSync(join(fakebin, "codex"));
    rmSync(join(fakebin, "claude"));
    assert.deepEqual(Object.values(fc("configure", "k\n")), [1, "", "Install Codex or Claude Code before configuring Firecrawl.\n"]);
    tool("codex", "exit 0");
    assert.deepEqual(Object.values(fc("configure", "  \n")), [2, "\n", "No API key provided.\n"]);
    assert.deepEqual(Object.values(fc("configure", "abc")), [1, "", ""]);
    assert.ok(!existsSync(secretFile()));
  });
});

describe("ws firecrawl mcp", () => {
  test("loads the key written by configure and hands stdin/stdout to npx", () => {
    assert.equal(fc("configure", "ab 1$x\n").code, 0);
    const r = fc("mcp", "hello\n");
    assert.deepEqual([r.code, r.err], [0, ""]);
    assert.equal(r.out, "npx -y firecrawl-mcp KEY=[ab 1$x] URL=[unset]\nstdin=[hello]\n");
  });

  test("shell file subset: comments, quotes, $'…', several assignments, unexported names", () => {
    seed("# saved\nexport FIRECRAWL_API_KEY=$'a\\tb'\"c\"'d e'\nFIRECRAWL_API_URL=http://x ; OTHER=1\n");
    let r = fc("mcp");
    assert.equal(r.code, 0, r.err);
    assert.equal(r.out, "npx -y firecrawl-mcp KEY=[a\tbcd e] URL=[unset]\nstdin=[]\n");
    r = fc("mcp", "", { FIRECRAWL_API_URL: "old" }); // an already-exported name is updated
    assert.match(r.out, /URL=\[http:\/\/x\]/);
  });

  test("the key may come from the environment instead of the file", () => {
    seed("export OTHER=1\n");
    assert.match(fc("mcp", "", { FIRECRAWL_API_KEY: "envkey" }).out, /KEY=\[envkey\]/);
  });

  test("errors: missing file, key missing, npx missing", () => {
    let r = fc("mcp");
    assert.deepEqual([r.code, r.out], [1, ""]);
    assert.equal(r.err, `Missing Firecrawl credentials: ${secretFile()}\nRun the workspace command: ./bin/configure-firecrawl\n`);
    seed("export OTHER=1\n");
    r = fc("mcp");
    assert.deepEqual([r.code, r.out], [1, ""]);
    assert.match(r.err, /FIRECRAWL_API_KEY is missing from .*firecrawl\.env\n$/);
    seed("export FIRECRAWL_API_KEY=k\n");
    rmSync(join(fakebin, "npx"));
    r = fc("mcp");
    assert.equal(r.code, 127);
    assert.match(r.err, /npx/);
  });

  test("commands in the file are refused, not run (deliberate: the script sourced it)", () => {
    const marker = join(base, "ran");
    seed(`export FIRECRAWL_API_KEY=$(touch ${marker})k\n`);
    const r = fc("mcp");
    assert.deepEqual([r.code, r.out], [1, ""]);
    assert.match(r.err, /firecrawl\.env:1: unsupported/);
    assert.ok(!existsSync(marker));
  });

  test("--help exits 0", () => {
    const r = fc("--help");
    assert.equal(r.code, 0);
    assert.match(r.out, /^Usage: ws firecrawl configure/);
  });
});
