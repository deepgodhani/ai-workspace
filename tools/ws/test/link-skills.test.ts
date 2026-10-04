// Black-box tests for `ws link-skills` (port of bin/link-skills).
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, lstatSync, mkdirSync, mkdtempSync, readlinkSync, realpathSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { afterEach, beforeEach, describe, test } from "node:test";

const ROOT = resolve(import.meta.dirname, "..");
const WS_BIN = process.env.WS_BIN ?? join(ROOT, "build", "ws");
let base: string;
let ws: string;
let home: string;

function linkSkills(args: string[] = []) {
  const env = { ...process.env, WS_ROOT: ws, HOME: home };
  delete env.ORCH_WORKSPACE;
  const r = spawnSync(WS_BIN, ["link-skills", ...args], { cwd: tmpdir(), encoding: "utf8", env });
  return { code: r.status, out: r.stdout, err: r.stderr };
}

function skill(name: string, withSkillMd = true) {
  mkdirSync(join(ws, "_shared/skills", name), { recursive: true });
  if (withSkillMd) writeFileSync(join(ws, "_shared/skills", name, "SKILL.md"), `# ${name}\n`);
}

const isLink = (p: string) => existsSync(p) || lstatSync(p, { throwIfNoEntry: false })?.isSymbolicLink();

beforeEach(() => {
  base = realpathSync(mkdtempSync(join(tmpdir(), "ws-skills-")));
  ws = join(base, "ws");
  home = join(base, "home");
  mkdirSync(join(ws, "_shared/skills"), { recursive: true });
  mkdirSync(home);
  writeFileSync(join(ws, "AGENTS.md"), "# root\n");
  skill("beta");
  skill("alpha");
});

afterEach(() => rmSync(base, { recursive: true, force: true }));

describe("ws link-skills", () => {
  test("links every skill: absolute for Claude/Codex, relative for Kiro, in name order", () => {
    skill("no-skill-md", false); // Kiro links it anyway; Claude/Codex need SKILL.md
    mkdirSync(join(ws, "_shared/skills/.hidden"));
    writeFileSync(join(ws, "_shared/skills/notes.txt"), "not a skill\n");
    const r = linkSkills();
    assert.equal(r.code, 0, r.err);
    assert.equal(r.err, "");
    assert.equal(
      r.out,
      [
        "link   ~/.claude/skills/alpha",
        "link   ~/.claude/skills/beta",
        "link   ~/.agents/skills/alpha",
        "link   ~/.agents/skills/beta",
        "link   .kiro/skills/alpha",
        "link   .kiro/skills/beta",
        "link   .kiro/skills/no-skill-md",
        "",
      ].join("\n"),
    );
    assert.equal(readlinkSync(join(home, ".claude/skills/alpha")), join(ws, "_shared/skills/alpha"));
    assert.equal(readlinkSync(join(home, ".agents/skills/beta")), join(ws, "_shared/skills/beta"));
    assert.equal(readlinkSync(join(ws, ".kiro/skills/alpha")), "../../_shared/skills/alpha");
    assert.ok(existsSync(join(ws, ".kiro/skills/alpha/SKILL.md")), "relative Kiro link resolves");
    assert.ok(!isLink(join(home, ".claude/skills/no-skill-md")));
    assert.ok(!isLink(join(ws, ".kiro/skills/.hidden")));
    assert.ok(!isLink(join(ws, ".kiro/skills/notes.txt")));
  });

  test("second run reports ok and changes nothing", () => {
    linkSkills();
    const r = linkSkills();
    assert.equal(r.code, 0, r.err);
    assert.equal(
      r.out,
      "ok     ~/.claude/skills/alpha\nok     ~/.claude/skills/beta\nok     ~/.agents/skills/alpha\nok     ~/.agents/skills/beta\n",
    );
  });

  test("never replaces existing entries: other link, dangling link, real folder, file", () => {
    mkdirSync(join(home, ".claude/skills"), { recursive: true });
    mkdirSync(join(home, ".agents/skills"), { recursive: true });
    mkdirSync(join(ws, ".kiro/skills"), { recursive: true });
    symlinkSync("/somewhere/else", join(home, ".claude/skills/alpha")); // dangling, points elsewhere
    mkdirSync(join(home, ".claude/skills/beta")); // a real folder
    writeFileSync(join(home, ".agents/skills/alpha"), "file\n");
    symlinkSync(join(ws, "_shared/skills/beta/"), join(home, ".agents/skills/beta")); // same target, different text
    writeFileSync(join(ws, ".kiro/skills/alpha"), "kiro file\n");
    const r = linkSkills();
    assert.equal(r.code, 0, r.err);
    assert.equal(
      r.out,
      [
        "skip   ~/.claude/skills/alpha (exists, points elsewhere)",
        "skip   ~/.claude/skills/beta (exists, points elsewhere)",
        "skip   ~/.agents/skills/alpha (exists, points elsewhere)",
        "skip   ~/.agents/skills/beta (exists, points elsewhere)",
        "link   .kiro/skills/beta",
        "",
      ].join("\n"),
    );
    assert.equal(readlinkSync(join(home, ".claude/skills/alpha")), "/somewhere/else");
    assert.ok(lstatSync(join(home, ".claude/skills/beta")).isDirectory());
    assert.ok(lstatSync(join(ws, ".kiro/skills/alpha")).isFile());
  });

  test("a skill that is itself a link to a folder is linked", () => {
    const outside = join(base, "external-skill");
    mkdirSync(outside);
    writeFileSync(join(outside, "SKILL.md"), "# ext\n");
    symlinkSync(outside, join(ws, "_shared/skills/ext"));
    const r = linkSkills();
    assert.equal(r.code, 0, r.err);
    assert.match(r.out, /^link   ~\/\.claude\/skills\/ext$/m);
    assert.equal(readlinkSync(join(home, ".claude/skills/ext")), join(ws, "_shared/skills/ext"));
  });

  test("an unwritable destination exits 1", () => {
    writeFileSync(join(home, ".claude"), "not a folder\n");
    const r = linkSkills();
    assert.equal(r.code, 1);
    assert.equal(r.out, "");
    assert.notEqual(r.err, "");
  });

  test("no skills: prints nothing and creates no stray links", () => {
    rmSync(join(ws, "_shared/skills"), { recursive: true });
    mkdirSync(join(ws, "_shared/skills"));
    const r = linkSkills();
    assert.deepEqual([r.code, r.out, r.err], [0, "", ""]);
    assert.ok(!isLink(join(ws, ".kiro/skills/*")), "the bash glob made a dangling '*' link here");
  });

  test("--help exits 0 without linking", () => {
    const r = linkSkills(["--help"]);
    assert.equal(r.code, 0);
    assert.match(r.out, /^Usage: ws link-skills\n/);
    assert.ok(!existsSync(join(home, ".claude")));
  });
});
