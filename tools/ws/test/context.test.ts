// Black-box tests for `ws context` (port of bin/workspace-context).
import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, realpathSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { after, before, describe, test } from "node:test";

const ROOT = resolve(import.meta.dirname, "..");
const WS_BIN = process.env.WS_BIN ?? join(ROOT, "build", "ws");
let ws: string;
let outside: string;

function context(args: string[], cwd = tmpdir()) {
  const env = { ...process.env, WS_ROOT: ws };
  delete env.ORCH_WORKSPACE;
  const r = spawnSync(WS_BIN, ["context", ...args], { cwd, encoding: "utf8", env });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

function put(rel: string, content = "x\n") {
  const p = join(ws, rel);
  mkdirSync(resolve(p, ".."), { recursive: true });
  writeFileSync(p, content);
}

const git = (cwd: string, ...a: string[]) =>
  execFileSync("git", ["-c", "user.email=t@example.com", "-c", "user.name=t", ...a], { cwd, encoding: "utf8" }).trim();

const USAGE = "Usage: ws context <target>\nExamples: projects/my-project, my-project, research/topics/mongodb\n";

before(() => {
  ws = realpathSync(mkdtempSync(join(tmpdir(), "ws-context-")));
  outside = realpathSync(mkdtempSync(join(tmpdir(), "ws-context-outside-")));
  put("AGENTS.md", "# root\n");
  mkdirSync(join(ws, "_shared"));
  // A project in its own Git repo, with nested instructions and one uncommitted change.
  put("projects/demo/AGENTS.md", "# demo\n");
  put("projects/demo/.ai/STATE.md", "12345\n"); // 6 bytes
  put("projects/demo/.ai/TASKS.md", "123\n"); // 4 bytes
  put("projects/demo/.ai/DECISIONS.md");
  put("projects/demo/README.md");
  mkdirSync(join(ws, "projects/demo/.ai/sessions"));
  mkdirSync(join(ws, "projects/demo/docs"));
  git(join(ws, "projects/demo"), "init", "-q", "-b", "main");
  git(join(ws, "projects/demo"), "add", "-A");
  git(join(ws, "projects/demo"), "commit", "-q", "-m", "init");
  put("projects/demo/new.txt");
  put("projects/demo/src/AGENTS.md", "# src\n");
  // Research topic whose startup files exceed the warning threshold.
  put("research/topics/big/SCOPE.md", "s\n");
  put("research/topics/big/STATE.md", "x".repeat(12001));
  put("research/topics/big/GAPS.md", "g\n");
  put("research/topics/big/SOURCES.md");
  mkdirSync(join(ws, "research/topics/big/raw"));
  // Agent with one verification case.
  put("agents/checker/.ai/STATE.md", "agent\n");
  put("agents/checker/cases/c1/STATUS.md", "ok\n");
  put("agents/checker/cases/c1/verification.yaml", "v: 1\n");
  mkdirSync(join(ws, "agents/checker/cases/c1/.runtime"));
  // Names for ambiguity / areas without Git.
  mkdirSync(join(ws, "projects/dup"), { recursive: true });
  mkdirSync(join(ws, "tools/dup"), { recursive: true });
  mkdirSync(join(ws, "knowledge/notes"), { recursive: true });
  mkdirSync(join(ws, "bin"));
  symlinkSync(outside, join(ws, "projects/escape"));
});

after(() => {
  rmSync(ws, { recursive: true, force: true });
  rmSync(outside, { recursive: true, force: true });
});

describe("ws context", () => {
  test("project in its own Git repo: full report", () => {
    const r = context(["projects/demo"]);
    assert.equal(r.code, 0, r.err);
    assert.equal(r.err, "");
    assert.equal(
      r.out,
      [
        `workspace: ${ws}`,
        "target: projects/demo",
        "type: project",
        "write_scope: projects/demo",
        "git_root: projects/demo",
        "git_branch: main",
        "git_changed_paths: 2", // new.txt and src/
        "instructions:",
        "  - AGENTS.md",
        "  - projects/demo/AGENTS.md",
        "required_context:",
        "  - projects/demo/.ai/STATE.md (6 bytes)",
        "  - projects/demo/.ai/TASKS.md (4 bytes)",
        "required_context_bytes: 10",
        "context_warning: none",
        "conditional_context:",
        "  - projects/demo/.ai/DECISIONS.md",
        "  - projects/demo/.ai/sessions",
        "  - projects/demo/docs",
        "  - projects/demo/README.md",
        "",
      ].join("\n"),
    );
  });

  test("bare name, trailing slash, and a subdirectory resolve within the workspace", () => {
    assert.match(context(["demo"]).out, /^target: projects\/demo$/m);
    assert.match(context(["projects/demo/"]).out, /^target: projects\/demo$/m);
    const sub = context(["projects/demo/src"]);
    assert.equal(sub.code, 0, sub.err);
    assert.match(sub.out, /^type: project$/m);
    assert.match(sub.out, /^git_root: projects\/demo$/m);
    assert.match(sub.out, /instructions:\n  - AGENTS\.md\n  - projects\/demo\/AGENTS\.md\n  - projects\/demo\/src\/AGENTS\.md\n/);
    assert.match(sub.out, /required_context:\n  - none\nrequired_context_bytes: 0\n/);
    assert.match(sub.out, /conditional_context:\n$/);
    assert.match(context([join(ws, "projects", "demo")]).out, /^target: projects\/demo$/m);
  });

  test("detached HEAD reports detached", () => {
    const repo = join(ws, "projects/demo");
    git(repo, "checkout", "-q", "--detach");
    try {
      assert.match(context(["demo"]).out, /^git_branch: detached$/m);
    } finally {
      git(repo, "checkout", "-q", "main");
    }
  });

  test("research topic adds topic files and warns above 12000 bytes", () => {
    const r = context(["big"]);
    assert.equal(r.code, 0, r.err);
    assert.match(r.out, /^type: research$/m);
    assert.match(r.out, /^git_root: none\ngit_branch: none\ngit_changed_paths: 0$/m);
    assert.match(
      r.out,
      /required_context:\n  - research\/topics\/big\/SCOPE\.md \(2 bytes\)\n  - research\/topics\/big\/STATE\.md \(12001 bytes\)\n  - research\/topics\/big\/GAPS\.md \(2 bytes\)\nrequired_context_bytes: 12005\ncontext_warning: compact startup files before loading all of them\n/,
    );
    assert.match(r.out, /conditional_context:\n  - research\/topics\/big\/SOURCES\.md\n  - research\/topics\/big\/raw\n$/);
  });

  test("agent case includes the agent's state and the case files", () => {
    const r = context(["agents/checker/cases/c1"]);
    assert.equal(r.code, 0, r.err);
    assert.match(r.out, /^type: agent-case$/m);
    assert.match(
      r.out,
      /required_context:\n  - agents\/checker\/\.ai\/STATE\.md \(6 bytes\)\n  - agents\/checker\/cases\/c1\/STATUS\.md \(3 bytes\)\n  - agents\/checker\/cases\/c1\/verification\.yaml \(5 bytes\)\nrequired_context_bytes: 14\n/,
    );
    assert.match(r.out, /conditional_context:\n  - agents\/checker\/cases\/c1\/\.runtime\n$/);
    assert.match(context(["checker"]).out, /^type: agent$/m);
  });

  test("types for the workspace root and other areas", () => {
    const root = context(["."]);
    assert.equal(root.code, 0, root.err);
    assert.match(root.out, /^target: \.\ntype: workspace\nwrite_scope: \.$/m);
    assert.match(root.out, /instructions:\n  - AGENTS\.md\nrequired_context:/);
    assert.match(context(["notes"]).out, /^type: knowledge$/m);
    assert.match(context(["knowledge"]).out, /^type: knowledge$/m);
    assert.match(context(["_shared"]).out, /^type: shared$/m);
    assert.match(context(["bin"]).out, /^type: workspace-area$/m);
  });

  test("works from any current directory", () => {
    assert.equal(context(["demo"], outside).out, context(["demo"], join(ws, "projects/demo/src")).out);
  });

  test("not found, ambiguous, and outside-workspace targets exit 1", () => {
    let r = context(["missing"]);
    assert.deepEqual([r.code, r.out, r.err], [1, "", "Target not found: missing\n"]);
    r = context(["dup"]);
    assert.deepEqual([r.code, r.out], [1, ""]);
    assert.equal(r.err, `Ambiguous target: dup\n  ${ws}/projects/dup\n  ${ws}/tools/dup\n`);
    r = context([outside]);
    assert.deepEqual([r.code, r.out, r.err], [1, "", `Target is outside the workspace: ${outside}\n`]);
    r = context(["escape"]);
    assert.deepEqual([r.code, r.err], [1, `Target is outside the workspace: ${outside}\n`]);
    r = context(["../"]);
    assert.equal(r.code, 1);
    assert.match(r.err, /^Target is outside the workspace: /);
    r = context([join(ws, "AGENTS.md")]);
    assert.deepEqual([r.code, r.err], [1, `Target not found: ${ws}/AGENTS.md\n`]);
  });

  test("wrong arguments exit 2 with usage; --help exits 0", () => {
    for (const args of [[], ["a", "b"], [""], ["/"]]) {
      const r = context(args);
      assert.deepEqual([r.code, r.out, r.err], [2, "", USAGE], JSON.stringify(args));
    }
    const h = context(["--help"]);
    assert.deepEqual([h.code, h.out, h.err], [0, USAGE, ""]);
  });
});
