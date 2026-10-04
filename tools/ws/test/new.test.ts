// Black-box tests for `ws new` (ports of bin/new-project, bin/new-research, bin/new-agent-case).
import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import { chmodSync, existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { after, before, describe, test } from "node:test";

const ROOT = resolve(import.meta.dirname, "..");
const WS_BIN = process.env.WS_BIN ?? join(ROOT, "build", "ws");
let ws: string;

function wsNew(args: string[]) {
  const env = { ...process.env, WS_ROOT: ws };
  delete env.ORCH_WORKSPACE;
  const r = spawnSync(WS_BIN, ["new", ...args], { cwd: tmpdir(), encoding: "utf8", env });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

function put(rel: string, content: string | Buffer = "x\n", mode?: number) {
  const p = join(ws, rel);
  mkdirSync(resolve(p, ".."), { recursive: true });
  writeFileSync(p, content);
  if (mode !== undefined) chmodSync(p, mode);
}

const read = (rel: string) => readFileSync(join(ws, rel), "utf8");
const mode = (rel: string) => statSync(join(ws, rel)).mode & 0o777;
const NUL_FILE = Buffer.from("bin\0{{CASE_NAME}}\n");
const BAD_UTF8 = Buffer.from([0x7b, 0x7b, 0x54, 0x4f, 0x50, 0x49, 0x43, 0x7d, 0x7d, 0xff, 0x0a]); // "{{TOPIC}}\xff\n"

function localDate() {
  const d = new Date();
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
}

before(() => {
  ws = realpathSync(mkdtempSync(join(tmpdir(), "ws-new-")));
  put("AGENTS.md", "# root\n");
  put("_shared/templates/project/AGENTS.md", "# {{PROJECT_NAME}} contract\nname={{PROJECT_NAME}}\n");
  put("_shared/templates/project/.gitignore", "build/\n");
  put("_shared/templates/project/.ai/STATE.md", "# State {{PROJECT_NAME}}\n"); // only AGENTS.md is filled
  put("_shared/templates/project/README.md", "template readme\n");
  put("_shared/templates/project/run.sh", "#!/bin/sh\n", 0o755);
  put("_shared/templates/research/SCOPE.md", "- Topic: {{TOPIC}}\n- Root URL: {{ROOT_URL}}\n- Started: {{DATE}}\n");
  put("_shared/templates/research/raw/nested.md", "{{TOPIC}} / {{DATE}}\n");
  put("_shared/templates/research/blob.bin", BAD_UTF8);
  put("agents/auditor/AGENTS.md", "# auditor\n");
  put("agents/auditor/cases/_template/target.yaml.example", "case: {{CASE_NAME}}\n");
  put("agents/auditor/cases/_template/STATUS.md", "# {{CASE_NAME}} {{CASE_NAME}}\n");
  put("agents/auditor/cases/_template/evidence/.gitkeep", "");
  put("agents/auditor/cases/_template/blob.bin", NUL_FILE);
  put("agents/auditor/cases/_template/run.sh", "#!/bin/sh\necho {{CASE_NAME}}\n", 0o755);
  mkdirSync(join(ws, "agents/coded"), { recursive: true });
  mkdirSync(join(ws, "projects"), { recursive: true });
  mkdirSync(join(ws, "research/topics"), { recursive: true });
});

after(() => rmSync(ws, { recursive: true, force: true }));

describe("ws new project", () => {
  test("copies the template, fills AGENTS.md, writes README, and inits Git", () => {
    const r = wsNew(["project", "My.App_1"]);
    assert.equal(r.code, 0, r.err);
    assert.equal(r.err, "");
    const target = join(ws, "projects/My.App_1");
    assert.equal(
      r.out,
      `Created project: ${target}\nNext from your workspace-root session:\n  $workspace-coach activate projects/My.App_1, initialize its state, and continue\n`,
    );
    assert.equal(read("projects/My.App_1/AGENTS.md"), "# My.App_1 contract\nname=My.App_1\n");
    assert.equal(read("projects/My.App_1/.ai/STATE.md"), "# State {{PROJECT_NAME}}\n");
    assert.equal(read("projects/My.App_1/.gitignore"), "build/\n");
    assert.equal(read("projects/My.App_1/README.md"), "# My.App_1\n\nDescribe the project, setup, commands, architecture, and usage here.\n");
    assert.equal(mode("projects/My.App_1/run.sh"), 0o755);
    assert.equal(execFileSync("git", ["-C", target, "rev-parse", "--show-toplevel"], { encoding: "utf8" }).trim(), target);
  });

  test("existing project exits 1 and changes nothing", () => {
    put("projects/taken/keep.txt", "mine\n");
    const r = wsNew(["project", "taken"]);
    assert.deepEqual([r.code, r.out, r.err], [1, "", `Project already exists: ${join(ws, "projects/taken")}\n`]);
    assert.equal(read("projects/taken/keep.txt"), "mine\n");
    assert.ok(!existsSync(join(ws, "projects/taken/AGENTS.md")));
  });

  test("invalid names and wrong argument counts exit 2", () => {
    const msg = "Use letters, numbers, dots, underscores, or hyphens; start with a letter or number.\n";
    for (const name of ["-bad", ".hidden", "a/b", "..", "a b", ""]) {
      const r = wsNew(["project", name]);
      assert.deepEqual([r.code, r.out, r.err], [2, "", msg], name);
    }
    for (const args of [["project"], ["project", "a", "b"]]) {
      assert.deepEqual(wsNew(args), { code: 2, out: "", err: "Usage: ws new project <project-name>\n" });
    }
    assert.ok(!existsSync(join(ws, "projects/a")));
  });
});

describe("ws new research", () => {
  test("slugifies the topic and fills every text file", () => {
    const topic = "  Héllo World: MongoDB 7!! ";
    const r = wsNew(["research", topic, "https://example.com/x"]);
    assert.equal(r.code, 0, r.err);
    const target = join(ws, "research/topics/h-llo-world-mongodb-7");
    assert.equal(
      r.out,
      `Created research topic: ${target}\nNext from your workspace-root session:\n  $workspace-coach activate research/topics/h-llo-world-mongodb-7 and review its scope\n`,
    );
    const date = localDate();
    assert.equal(read("research/topics/h-llo-world-mongodb-7/SCOPE.md"), `- Topic: ${topic}\n- Root URL: https://example.com/x\n- Started: ${date}\n`);
    assert.equal(read("research/topics/h-llo-world-mongodb-7/raw/nested.md"), `${topic} / ${date}\n`);
    assert.deepEqual(readFileSync(join(target, "blob.bin")), BAD_UTF8); // not valid UTF-8: left alone
  });

  test("root URL is optional", () => {
    const r = wsNew(["research", "Plain"]);
    assert.equal(r.code, 0, r.err);
    assert.match(read("research/topics/plain/SCOPE.md"), /^- Root URL: \n/m);
  });

  test("existing topic exits 1; empty slug and wrong arguments exit 2", () => {
    let r = wsNew(["research", "PLAIN!"]);
    assert.deepEqual([r.code, r.err], [1, `Research topic already exists: ${join(ws, "research/topics/plain")}\n`]);
    for (const topic of ["!!!", "", "---"]) {
      r = wsNew(["research", topic]);
      assert.deepEqual([r.code, r.out, r.err], [2, "", "Could not create a valid topic slug.\n"], topic);
    }
    for (const args of [["research"], ["research", "a", "b", "c"]]) {
      assert.deepEqual(wsNew(args), { code: 2, out: "", err: "Usage: ws new research <topic-name> [root-url]\n" });
    }
  });
});

describe("ws new agent-case", () => {
  test("copies the template, renames *.example, fills text files, keeps modes", () => {
    const r = wsNew(["agent-case", "auditor", "c1"]);
    assert.equal(r.code, 0, r.err);
    const agent = join(ws, "agents/auditor");
    const target = join(agent, "cases/c1");
    assert.equal(
      r.out,
      `\nCreated case: ${target}\nNext from your workspace-root session:\n` +
        `  1. Fill in ${target}/target.yaml.\n  2. Read ${agent}/AGENTS.md for the procedure.\n` +
        `  3. Activate ${target} with $workspace-coach and run it.\n`,
    );
    assert.equal(read("agents/auditor/cases/c1/target.yaml"), "case: c1\n");
    assert.ok(!existsSync(join(target, "target.yaml.example")));
    assert.equal(read("agents/auditor/cases/c1/STATUS.md"), "# c1 c1\n");
    assert.equal(read("agents/auditor/cases/c1/run.sh"), "#!/bin/sh\necho c1\n");
    assert.equal(mode("agents/auditor/cases/c1/run.sh"), 0o755);
    assert.equal(mode("agents/auditor/cases/c1/STATUS.md"), 0o644);
    assert.ok(existsSync(join(target, "evidence/.gitkeep")));
    assert.deepEqual(readFileSync(join(target, "blob.bin")), NUL_FILE); // binary: left alone
    assert.ok(existsSync(join(agent, "cases/_template/target.yaml.example")), "template untouched");
  });

  test("missing agent, missing template, and existing case exit 1", () => {
    let r = wsNew(["agent-case", "nope", "c2"]);
    assert.deepEqual([r.code, r.out, r.err], [1, "", `Agent not found: ${join(ws, "agents/nope")}\n`]);
    r = wsNew(["agent-case", "coded", "c2"]);
    assert.deepEqual(
      [r.code, r.err],
      [1, `No case template at ${join(ws, "agents/coded/cases/_template")} — this agent may use its own CLI to create cases (see its AGENTS.md).\n`],
    );
    r = wsNew(["agent-case", "auditor", "c1"]);
    assert.deepEqual([r.code, r.err], [1, `Case already exists: ${join(ws, "agents/auditor/cases/c1")}\n`]);
  });

  test("invalid case names and wrong argument counts exit 2", () => {
    const msg = "Use lowercase letters, numbers, and hyphens for the case name; start with a letter or number.\n";
    for (const name of ["Bad", "-x", "a_b", "a.b", ""]) {
      const r = wsNew(["agent-case", "auditor", name]);
      assert.deepEqual([r.code, r.out, r.err], [2, "", msg], name);
    }
    const usage = "Usage: ws new agent-case <agent-name> <case-name>\nExample: ws new agent-case repo-auditor my-project-audit\n";
    for (const args of [["agent-case"], ["agent-case", "auditor"], ["agent-case", "a", "b", "c"]]) {
      assert.deepEqual(wsNew(args), { code: 2, out: "", err: usage });
    }
  });
});

describe("ws new", () => {
  test("no kind or an unknown kind exits 2; --help exits 0", () => {
    let r = wsNew([]);
    assert.equal(r.code, 2);
    assert.match(r.err, /^Usage: ws new <kind>/);
    r = wsNew(["widget"]);
    assert.equal(r.code, 2);
    assert.match(r.err, /^ws new: unknown kind 'widget'\n/);
    for (const args of [["--help"], ["project", "--help"]]) {
      r = wsNew(args);
      assert.equal(r.code, 0);
      assert.match(r.out, /ws new project <project-name>\n  ws new research <topic-name> \[root-url\]\n  ws new agent-case <agent-name> <case-name>\n/);
    }
  });
});
