import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { after, before, describe, test } from "node:test";
const PER_FILE_CHARS = 4000;

const ROOT = resolve(import.meta.dirname, "..");
const WS_BIN = process.env.WS_BIN ?? join(ROOT, "build", "ws");
const FAKES = join(ROOT, "test", "fakes");
let ws: string;
let log: string;

function orch(args: string[], extraEnv: Record<string, string> = {}, input?: string) {
  const r = spawnSync(WS_BIN, ["orch", ...args], {
    cwd: ws,
    input,
    encoding: "utf8",
    env: { ...process.env, ORCH_WORKSPACE: ws, ORCH_CLAUDE_BIN: join(FAKES, "claude"), ORCH_CODEX_BIN: join(FAKES, "codex"), ORCH_KIRO_BIN: join(FAKES, "kiro"), ORCH_SRT_BIN: join(FAKES, "srt"), FAKE_LOG: log, ...extraEnv },
  });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

function lastCall(): { cwd: string; args: string[]; prompt: string } {
  const lines = readFileSync(log, "utf8").trim().split("\n");
  return JSON.parse(lines.at(-1)!);
}

function kiroCalls(method: string): any[] {
  return readFileSync(log, "utf8").trim().split("\n").map((l) => JSON.parse(l)).filter((e) => e.method === method);
}

function idOf(out: string): string {
  const m = out.match(/^orch ([0-9a-f]{8}):/m);
  assert.ok(m, `no orch id in output:\n${out}`);
  return m[1];
}

const git = (cwd: string, ...a: string[]) => execFileSync("git", a, { cwd, encoding: "utf8" }).trim();

before(() => {
  ws = realpathSync(mkdtempSync(join(tmpdir(), "orch-test-")));
  log = join(ws, "fake-calls.jsonl");
  writeFileSync(join(ws, "AGENTS.md"), "# test workspace\n");
  mkdirSync(join(ws, "_shared"));
  mkdirSync(join(ws, "worktrees"));
  const topic = join(ws, "research", "topics", "t1");
  mkdirSync(topic, { recursive: true });
  writeFileSync(join(topic, "STATE.md"), "# State\nTopic state line.\n" + "x".repeat(PER_FILE_CHARS + 500));
  writeFileSync(join(topic, "CONTEXT.md"), "# Context\nKey fact.\n");
  const proj = join(ws, "projects", "demo");
  mkdirSync(join(proj, ".ai"), { recursive: true });
  writeFileSync(join(proj, ".ai", "STATE.md"), "# Demo state\nNext: add feature.\n");
  writeFileSync(join(proj, "README.md"), "demo\n");
  git(proj, "init", "-q", "-b", "main");
  git(proj, "-c", "user.email=t@example.com", "-c", "user.name=t", "add", "-A");
  git(proj, "-c", "user.email=t@example.com", "-c", "user.name=t", "commit", "-q", "-m", "init");
});

after(() => rmSync(ws, { recursive: true, force: true }));

function dryRunPacket(args: string[]): string {
  const out = orch(["spawn", ...args, "--dry-run"]).out;
  const k = out.indexOf("tokens)\n\n");
  assert.ok(k > 0, out);
  return out.slice(k + "tokens)\n\n".length);
}

function parse(text: string): any {
  return JSON.parse(orch(["_parse"], {}, text).out);
}

describe("packet", () => {
  test("includes task, scope, startup files, and truncates large files", () => {
    const p = dryRunPacket(["research/topics/t1", "--task", "Do X"]);
    assert.match(p, /# Task\n\nDo X/);
    assert.match(p, /Read-only: do not create, edit, or delete any file\./);
    assert.match(p, /## STATE\.md \(truncated\)/);
    assert.match(p, /## CONTEXT\.md\n\n# Context/);
    assert.ok(p.length < 12000, `packet too large: ${p.length}`);
  });

  test("parseResult accepts raw JSON and fenced JSON, rejects incomplete objects", () => {
    const r = { status: "done", summary: "s", files_changed: [], decisions: [], checks: [], open_issues: [], next_action: "n" };
    assert.deepEqual(parse(JSON.stringify(r)), r);
    assert.deepEqual(parse("text\n```json\n" + JSON.stringify(r) + "\n```\nmore"), r);
    assert.equal(parse(JSON.stringify({ status: "done" })), null);
    assert.equal(parse(JSON.stringify({ ...r, status: "maybe" })), null);
  });
});

describe("spawn (claude, read-only)", () => {
  test("runs in the target with read-only tools and records usage", () => {
    const r = orch(["spawn", "research/topics/t1", "--task", "Summarize"]);
    assert.equal(r.code, 0, r.err + r.out);
    const id = idOf(r.out);
    assert.match(r.out, /done — fake run in/);
    const call = lastCall();
    assert.equal(call.cwd, join(ws, "research", "topics", "t1"));
    assert.ok(call.args.includes("--tools") && call.args.includes("Read"));
    assert.deepEqual(call.args.slice(call.args.indexOf("--permission-prompts"), call.args.indexOf("--permission-prompts") + 2), ["--permission-prompts", "none"]);
    assert.ok(!call.args.includes("acceptEdits"));
    for (const f of ["--disable-slash-commands", "--strict-mcp-config", "--exclude-dynamic-system-prompt-sections"]) assert.ok(call.args.includes(f), `missing lean flag ${f}`);
    assert.match(call.prompt, /Topic state line\./);
    const show = JSON.parse(orch(["show", id]).out);
    assert.equal(show.status, "done");
    assert.equal(show.cache_read_tokens, 1000);
    assert.equal(show.input_tokens, 110);
    assert.equal(show.cost_usd, 0.0123);
    assert.equal(show.cli_session_id, call.args[call.args.indexOf("--session-id") + 1]);
  });

  test("--full keeps skills and MCP servers", () => {
    const r = orch(["spawn", "research/topics/t1", "--task", "Summarize", "--full"]);
    assert.equal(r.code, 0, r.err);
    assert.ok(!lastCall().args.includes("--strict-mcp-config"));
  });

  test("falls back to a fenced JSON block in plain text output", () => {
    const r = orch(["spawn", "research/topics/t1", "--task", "Summarize"], { FAKE_MODE: "text" });
    assert.equal(r.code, 0, r.err + r.out);
    assert.match(r.out, /: done —/);
  });

  test("reports CLI errors with exit 1 and records status error", () => {
    const r = orch(["spawn", "research/topics/t1", "--task", "Summarize", "--budget", "0.01"], { FAKE_MODE: "error" });
    assert.equal(r.code, 1);
    assert.match(r.out, /error — budget exceeded/);
    const call = lastCall();
    assert.ok(call.args.includes("--max-budget-usd"));
  });

  test("reads the task from stdin with --task -", () => {
    const r = orch(["spawn", "research/topics/t1", "--task", "-"], {}, "Task from stdin\n");
    assert.equal(r.code, 0, r.err);
    assert.match(lastCall().prompt, /Task from stdin/);
  });
});

describe("spawn (codex)", () => {
  test("uses exec with read-only sandbox, -C target, prompt on stdin", () => {
    const r = orch(["spawn", "research/topics/t1", "--cli", "codex", "--task", "Summarize"]);
    assert.equal(r.code, 0, r.err + r.out);
    const id = idOf(r.out);
    const a = lastCall().args;
    assert.equal(a[0], "exec");
    assert.equal(a[a.indexOf("-s") + 1], "read-only");
    assert.equal(a[a.indexOf("-C") + 1], join(ws, "research", "topics", "t1"));
    assert.equal(a.at(-1), "-");
    const show = JSON.parse(orch(["show", id]).out);
    assert.equal(show.status, "partial");
    assert.equal(show.cli_session_id, "thread-fake-1");
    assert.equal(show.input_tokens, 100);
    assert.equal(show.cache_read_tokens, 400);
  });
});

describe("write mode", () => {
  test("edits only inside a new worktree; diff and close work", () => {
    const proj = join(ws, "projects", "demo");
    const r = orch(["spawn", "projects/demo", "--write", "--task", "Add file", "--allow", "Bash(npm test:*)"]);
    assert.equal(r.code, 0, r.err + r.out);
    const id = idOf(r.out);
    const show = JSON.parse(orch(["show", id]).out);
    assert.equal(show.mode, "write");
    assert.equal(show.branch, `orch/${id}`);
    assert.ok(show.worktree.startsWith(join(ws, "worktrees")));
    assert.ok(existsSync(join(show.worktree, "FAKE_EDIT.txt")), "edit missing in worktree");
    assert.ok(!existsSync(join(proj, "FAKE_EDIT.txt")), "main checkout was modified");
    assert.equal(git(proj, "status", "--porcelain"), "");
    const a = lastCall().args;
    assert.ok(a.includes("acceptEdits") && a.includes("Bash(npm test:*)"));
    assert.match(orch(["diff", id]).out, /FAKE_EDIT\.txt/);
    const blocked = orch(["close", id]);
    assert.equal(blocked.code, 2);
    assert.match(blocked.err, /uncommitted changes/);
    const closed = orch(["close", id, "--force", "--delete-branch"]);
    assert.equal(closed.code, 0, closed.err);
    assert.ok(!existsSync(show.worktree));
    assert.equal(git(proj, "branch", "--list", `orch/${id}`), "");
  });

  test("refuses --write outside a Git repository", () => {
    const r = orch(["spawn", "research/topics/t1", "--write", "--task", "x"]);
    assert.equal(r.code, 2);
    assert.match(r.err, /needs the target inside a Git repository/);
  });
});

describe("resume and registry", () => {
  test("resume continues the same CLI session and links the parent", () => {
    const first = orch(["spawn", "research/topics/t1", "--task", "Step 1"]);
    const id = idOf(first.out);
    const sid = JSON.parse(orch(["show", id]).out).cli_session_id;
    const second = orch(["resume", id, "--task", "Step 2"]);
    assert.equal(second.code, 0, second.err + second.out);
    const a = lastCall().args;
    assert.equal(a[a.indexOf("--resume") + 1], sid);
    const child = JSON.parse(orch(["show", idOf(second.out)]).out);
    assert.equal(child.parent_id, id);
    assert.match(orch(["list"]).out, new RegExp(`${idOf(second.out)}.*${id}`));
  });

  test("refuses to resume a session that is still running", () => {
    const id = idOf(orch(["spawn", "research/topics/t1", "--task", "x"]).out);
    execFileSync("node", ["-e", `const {DatabaseSync}=require("node:sqlite"); new DatabaseSync(process.argv[1]).prepare("update sessions set status='running' where id=?").run(process.argv[2])`, join(ws, ".orch", "orch.db"), id]);
    const r = orch(["resume", id, "--task", "y"]);
    assert.equal(r.code, 2);
    assert.match(r.err, /still running/);
  });

  test("rejects the workspace root and paths outside it", () => {
    assert.match(orch(["spawn", ".", "--task", "x"]).err, /inside the workspace/);
    assert.match(orch(["spawn", tmpdir(), "--task", "x"]).err, /inside the workspace/);
  });
});

describe("merge", () => {
  test("merges a Claude result into a Codex session (cross-CLI) and records lineage", () => {
    const src = idOf(orch(["spawn", "research/topics/t1", "--task", "Find facts"]).out);
    const dst = idOf(orch(["spawn", "research/topics/t1", "--cli", "codex", "--task", "Plan"]).out);
    const r = orch(["merge", src, "--into", dst, "--task", "Compare with your plan"]);
    assert.equal(r.code, 0, r.err + r.out);
    const call = lastCall();
    assert.deepEqual(call.args.slice(0, 2), ["exec", "resume"]);
    assert.equal(call.args.at(-2), "thread-fake-1");
    assert.match(call.prompt, /# Results merged from other sessions/);
    assert.match(call.prompt, new RegExp(`## Session ${src} — claude, read`));
    assert.match(call.prompt, /Summary: fake run in/);
    assert.match(call.prompt, /# What to do\n\nCompare with your plan/);
    const row = JSON.parse(orch(["show", idOf(r.out)]).out);
    assert.equal(row.parent_id, dst);
    assert.equal(row.merged_from, src);
    assert.equal(row.cli, "codex");
  });

  test("merges several sources, ignores duplicates, uses the default task", () => {
    const a = idOf(orch(["spawn", "research/topics/t1", "--task", "A"]).out);
    const b = idOf(orch(["spawn", "research/topics/t1", "--cli", "codex", "--task", "B"]).out);
    const dst = idOf(orch(["spawn", "research/topics/t1", "--task", "D"]).out);
    const r = orch(["merge", a, b, a, "--into", dst]);
    assert.equal(r.code, 0, r.err + r.out);
    const p = lastCall().prompt;
    assert.equal(p.match(/## Session /g)?.length, 2);
    assert.match(p, /Integrate these results into your current work/);
    assert.equal(JSON.parse(orch(["show", idOf(r.out)]).out).merged_from, `${a},${b}`);
  });

  test("points to a writing source's worktree instead of copying code", () => {
    const src = idOf(orch(["spawn", "projects/demo", "--write", "--task", "Edit"]).out);
    const dst = idOf(orch(["spawn", "research/topics/t1", "--task", "Review"]).out);
    const r = orch(["merge", src, "--into", dst]);
    assert.equal(r.code, 0, r.err + r.out);
    assert.match(lastCall().prompt, new RegExp(`Code: changes live in worktree .*orch/${src}`));
    assert.equal(orch(["close", src, "--force", "--delete-branch"]).code, 0);
  });

  test("refuses bad merges", () => {
    const ok = idOf(orch(["spawn", "research/topics/t1", "--task", "x"]).out);
    const bad = idOf(orch(["spawn", "research/topics/t1", "--task", "x"], { FAKE_MODE: "error" }).out);
    assert.match(orch(["merge", ok]).err, /needs --into/);
    assert.match(orch(["merge", bad, "--into", ok]).err, /has no result to merge/);
    assert.match(orch(["merge", ok, "--into", ok]).err, /destination session itself/);
    const child = idOf(orch(["resume", ok, "--task", "more"]).out);
    assert.match(orch(["merge", ok, "--into", child]).err, /destination session itself/);
    orch(["close", ok]);
    const other = idOf(orch(["spawn", "research/topics/t1", "--task", "y"]).out);
    assert.match(orch(["merge", other, "--into", ok]).err, /is closed/);
  });
});

describe("resume keeps the session's settings", () => {
  test("allow list, model and --full carry over", () => {
    const id = idOf(orch(["spawn", "projects/demo", "--write", "--full", "--model", "sonnet", "--allow", "Bash(npm test:*)", "--task", "x"]).out);
    const r = orch(["resume", id, "--task", "y"]);
    assert.equal(r.code, 0, r.err + r.out);
    const a = lastCall().args;
    assert.ok(a.includes("Bash(npm test:*)"), "allow list dropped on resume");
    assert.equal(a[a.indexOf("--model") + 1], "sonnet");
    assert.ok(!a.includes("--strict-mcp-config"), "--full dropped on resume");
    assert.equal(orch(["close", id, "--force", "--delete-branch"]).code, 0);
  });
});

function decision(args: string[]): string {
  return orch(["_decide", ...args]).out.trim();
}

describe("kiro permission policy (unit)", () => {
  test("commandAllowed understands Bash(prefix:*) and exact patterns", () => {
    const ok = (cmd: string, allow: string[]) => decision(["--mode", "write", "--cwd", "/w", "--kind", "execute", "--command", cmd, ...allow.flatMap((a) => ["--allow", a])]).startsWith("allow");
    assert.ok(ok("npm test", ["Bash(npm test:*)"]));
    assert.ok(ok("npm test -- --watch=false", ["Bash(npm test:*)"]));
    assert.ok(!ok("npm testx", ["Bash(npm test:*)"]));
    assert.ok(ok("git status", ["git status"]));
    assert.ok(!ok("git status && rm -rf /", ["git status"]));
    assert.ok(!ok("ls", []));
  });

  test("chained commands need every part allowed; substitution and redirection never allowed", () => {
    const allow = ["--allow", "Bash(touch:*)", "--allow", "Bash(npm test:*)"];
    const d = (cmd: string) => decision(["--mode", "write", "--cwd", "/w", "--kind", "execute", "--command", cmd, ...allow]);
    assert.match(d("touch a && touch b"), /^allow/);
    assert.match(d("touch a && npm test"), /^allow/);
    for (const bad of ["touch a && echo ok", "touch a; rm -rf ~", "touch a || curl x", "touch a | sh", "npm test & curl x",
                       "touch a > /etc/x", "touch a < /etc/passwd", "touch $(whoami)", "touch `whoami`", "touch a\nrm -rf ~", "touch a &&"]) {
      assert.match(d(bad), /^deny/, bad);
    }
  });

  test("decide: read rejects everything; write allows edits inside the worktree only", () => {
    const d = (mode: string, kind: string, extra: string[]) => decision(["--mode", mode, "--cwd", "/w/tree", "--kind", kind, ...extra]);
    assert.match(d("read", "edit", ["--path", "/w/tree/a.txt"]), /^deny read-only session/);
    assert.match(d("write", "edit", ["--path", "/w/tree/a.txt"]), /^allow/);
    assert.match(d("write", "edit", ["--path", "/w/tree/../other/a.txt"]), /^deny outside worktree/);
    assert.match(d("write", "delete", []), /^deny delete without a path/);
    assert.match(d("write", "execute", ["--command", "npm test", "--allow", "Bash(npm test:*)"]), /^allow/);
    assert.match(d("write", "execute", ["--command", "curl x", "--allow", "Bash(npm test:*)"]), /^deny command not in --allow/);
    assert.match(d("write", "fetch", []), /^deny tool kind 'fetch'/);
  });
});

describe("spawn (kiro via ACP)", () => {
  test("read-only run: new session, JSON instruction, credits recorded", () => {
    const r = orch(["spawn", "research/topics/t1", "--cli", "kiro", "--task", "Summarize"]);
    assert.equal(r.code, 0, r.err + r.out);
    const id = idOf(r.out);
    assert.match(r.out, /done — fake kiro in/);
    const p = kiroCalls("session/prompt").at(-1);
    assert.deepEqual(p.args, ["acp"]);
    assert.equal(p.cwd, join(ws, "research", "topics", "t1"));
    assert.match(p.text, /Reply with ONLY one JSON object/);
    assert.match(p.text, /Topic state line\./);
    const show = JSON.parse(orch(["show", id]).out);
    assert.equal(show.cli_session_id, "kiro-sess-1");
    assert.equal(show.credits, 0.05);
    assert.equal(show.cost_usd, null);
    assert.match(orch(["list"]).out, /0\.05cr/);
  });

  test("sends one repair prompt when the reply is not JSON", () => {
    const r = orch(["spawn", "research/topics/t1", "--cli", "kiro", "--task", "x"], { FAKE_MODE: "repair" });
    assert.equal(r.code, 0, r.err + r.out);
    const prompts = kiroCalls("session/prompt").slice(-2);
    assert.match(prompts[1].text, /not a JSON object matching the schema/);
    assert.equal(JSON.parse(orch(["show", idOf(r.out)]).out).credits, 0.1);
  });

  test("read-only denies edits and shell commands", () => {
    const t = join(ws, "research", "topics", "t1");
    const r = orch(["spawn", "research/topics/t1", "--cli", "kiro", "--task", "x"], { FAKE_KIRO_ACTION: "write:NEW.txt" });
    assert.equal(r.code, 0, r.err + r.out);
    assert.equal(kiroCalls("session/prompt").at(-1).decision, "reject_once");
    assert.ok(!existsSync(join(t, "NEW.txt")));
    assert.match(r.out, /denied 1 tool request\(s\): Creating file \(read-only session\)/);
    orch(["spawn", "research/topics/t1", "--cli", "kiro", "--task", "x"], { FAKE_KIRO_ACTION: "exec:ls" });
    assert.equal(kiroCalls("session/prompt").at(-1).decision, "reject_once");
  });

  test("write mode: edits allowed only inside the worktree; shell needs --allow", () => {
    const proj = join(ws, "projects", "demo");
    const r = orch(["spawn", "projects/demo", "--cli", "kiro", "--write", "--task", "x"], { FAKE_KIRO_ACTION: "write:NEW.txt" });
    assert.equal(r.code, 0, r.err + r.out);
    const id = idOf(r.out);
    const wt = JSON.parse(orch(["show", id]).out).worktree;
    assert.equal(kiroCalls("session/prompt").at(-1).decision, "allow_once");
    assert.ok(existsSync(join(wt, "NEW.txt")));
    assert.ok(!existsSync(join(proj, "NEW.txt")));
    const out = orch(["spawn", "projects/demo", "--cli", "kiro", "--write", "--task", "x"], { FAKE_KIRO_ACTION: `write:${join(ws, "escape.txt")}` });
    assert.equal(kiroCalls("session/prompt").at(-1).decision, "reject_once");
    assert.ok(!existsSync(join(ws, "escape.txt")));
    const ok = orch(["spawn", "projects/demo", "--cli", "kiro", "--write", "--allow", "Bash(npm test:*)", "--task", "x"], { FAKE_KIRO_ACTION: "exec:npm test" });
    assert.equal(kiroCalls("session/prompt").at(-1).decision, "allow_once");
    const no = orch(["spawn", "projects/demo", "--cli", "kiro", "--write", "--allow", "Bash(npm test:*)", "--task", "x"], { FAKE_KIRO_ACTION: "exec:rm -rf /" });
    assert.equal(kiroCalls("session/prompt").at(-1).decision, "reject_once");
    for (const o of [r, out, ok, no]) orch(["close", idOf(o.out), "--force", "--delete-branch"]);
  });

  test("resume uses session/load and ignores replayed history", () => {
    const id = idOf(orch(["spawn", "research/topics/t1", "--cli", "kiro", "--task", "first"]).out);
    const r = orch(["resume", id, "--task", "second"]);
    assert.equal(r.code, 0, r.err + r.out);
    assert.equal(kiroCalls("session/load").at(-1).params.sessionId, "kiro-sess-1");
    assert.doesNotMatch(r.out, /REPLAYED HISTORY/);
    assert.match(kiroCalls("session/prompt").at(-1).text, /^second/);
  });

  test("read-only backstop flags a child that writes without asking", () => {
    const proj = join(ws, "projects", "demo");
    const r = orch(["spawn", "projects/demo", "--cli", "kiro", "--task", "x"], { FAKE_MODE: "sneaky" });
    assert.equal(r.code, 1);
    assert.match(r.out, /violation/);
    assert.match(JSON.parse(orch(["show", idOf(r.out)]).out).error, /SNEAKY\.txt/);
    rmSync(join(proj, "SNEAKY.txt"));
  });
});

describe("sandbox tiers", () => {
  test("auto: kiro runs inside srt; read-only profile gives no write access to the target", () => {
    const r = orch(["spawn", "research/topics/t1", "--cli", "kiro", "--task", "x"]);
    assert.equal(r.code, 0, r.err + r.out);
    assert.match(r.out, /sandbox srt/);
    const s = kiroCalls("srt").at(-1);
    assert.equal(s.wrapped[0], join(FAKES, "kiro"));
    assert.deepEqual(s.wrapped.slice(1), ["acp"]);
    const t = join(ws, "research", "topics", "t1");
    assert.ok(!s.settings.filesystem.allowWrite.some((p: string) => p === t || t.startsWith(p + "/")) || s.settings.filesystem.allowWrite.some((p: string) => p.includes("tmp") || p.includes("folders")),
      "target must not be writable except through temp allowances");
    assert.ok(!s.settings.filesystem.allowWrite.includes(t));
    assert.deepEqual(s.settings.filesystem.denyRead, ["~/.ssh", "~/.gnupg"]);
    assert.ok(s.settings.network.allowedDomains.includes("*.kiro.dev"));
  });

  test("write mode profile allows the worktree and its git dir", () => {
    const r = orch(["spawn", "projects/demo", "--cli", "kiro", "--write", "--task", "x"]);
    assert.equal(r.code, 0, r.err + r.out);
    const id = idOf(r.out);
    const wt = JSON.parse(orch(["show", id]).out).worktree;
    const allow = kiroCalls("srt").at(-1).settings.filesystem.allowWrite;
    assert.ok(allow.includes(wt));
    assert.ok(allow.some((p: string) => p.includes(join(".git", "worktrees"))), `no git dir in ${allow}`);
    orch(["close", id, "--force", "--delete-branch"]);
  });

  test("resume keeps the sandbox tier", () => {
    const id = idOf(orch(["spawn", "research/topics/t1", "--cli", "kiro", "--task", "x"]).out);
    const before = kiroCalls("srt").length;
    assert.equal(orch(["resume", id, "--task", "y"]).code, 0);
    assert.equal(kiroCalls("srt").length, before + 1);
  });

  test("claude native adds its sandbox settings; none adds nothing", () => {
    orch(["spawn", "research/topics/t1", "--task", "x"]);
    let a = lastCall().args;
    assert.equal(JSON.parse(a[a.indexOf("--settings") + 1]).sandbox.allowUnsandboxedCommands, false);
    orch(["spawn", "research/topics/t1", "--task", "x", "--sandbox", "none"]);
    a = lastCall().args;
    assert.ok(!a.includes("--settings"));
  });

  test("claude can run inside srt with its own domains", () => {
    const r = orch(["spawn", "research/topics/t1", "--task", "x", "--sandbox", "srt"]);
    assert.equal(r.code, 0, r.err + r.out);
    const s = kiroCalls("srt").at(-1);
    assert.equal(s.wrapped[0], join(FAKES, "claude"));
    assert.ok(s.settings.network.allowedDomains.includes("api.anthropic.com"));
  });

  test("rejects unsupported combinations before creating anything", () => {
    assert.match(orch(["spawn", "research/topics/t1", "--cli", "kiro", "--sandbox", "native", "--task", "x"]).err, /no native OS sandbox/);
    assert.match(orch(["spawn", "research/topics/t1", "--cli", "codex", "--sandbox", "srt", "--task", "x"]).err, /untested/);
    assert.match(orch(["spawn", "research/topics/t1", "--sandbox", "vm", "--task", "x"]).err, /--sandbox must be/);
  });
});

test("never passes permission-bypass flags to any child", () => {
  const all = readFileSync(log, "utf8").trim().split("\n").map((l) => JSON.parse(l)).filter((e) => Array.isArray(e.args) || Array.isArray(e.wrapped)).map((e) => (e.args ?? e.wrapped).join(" "));
  for (const a of all) assert.doesNotMatch(a, /dangerously|bypassPermissions|trust-all/);
});
