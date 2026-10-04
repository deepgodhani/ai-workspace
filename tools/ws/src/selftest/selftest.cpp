// `ws selftest` — port of bin/selftest.
// End-to-end check of the framework on a throwaway copy of the workspace with an isolated
// HOME. Touches nothing outside a temp directory. Exit code = number of failures.
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "commands.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws {
namespace {

const char* SELFTEST_USAGE =
    "Usage: ws selftest\n"
    "Copies the workspace (without projects, research topics, knowledge, worktrees, and\n"
    "build output) to a temp folder with an isolated HOME, builds ws there, and runs the\n"
    "bin/ tools end to end. Exit code = number of failures.\n";

// rsync --exclude rules of the script. A rule without '/' matches a name at any depth; a
// rule with '/' matches the trailing components of the path. Excluded folders are pruned.
const std::vector<std::string> EXCLUDE_NAMES = {".git", "projects", "knowledge", "temp-sessions", "worktrees", ".orch"};
const std::vector<std::string> EXCLUDE_PATHS = {"research/topics", "tools/local-research/.venv", "tools/local-research/work",
                                                "tools/ws/build"};

bool excluded(const std::string& rel, const std::string& name) {
  for (const auto& n : EXCLUDE_NAMES)
    if (name == n) return true;
  for (const auto& p : EXCLUDE_PATHS) {
    if (rel == p) return true;
    if (rel.size() > p.size() && rel.compare(rel.size() - p.size(), p.size(), p) == 0 && rel[rel.size() - p.size() - 1] == '/')
      return true;
  }
  return false;
}

void warn(const std::string& msg) { std::cerr << "ws selftest: " << msg << "\n"; }

// `rsync -a` of the parts we use: folders, regular files (mode and mtime kept), and
// symlinks (copied as links). Errors are reported and skipped, as rsync did.
void copy_tree(const fs::path& src, const fs::path& dst, const std::string& rel) {
  std::error_code ec;
  fs::create_directories(dst, ec);
  if (ec) return warn("mkdir " + dst.string() + ": " + ec.message());
  fs::directory_iterator it(src, ec);
  if (ec) return warn("read " + src.string() + ": " + ec.message());
  for (const auto& e : it) {
    const std::string name = e.path().filename().string();
    const std::string r = rel.empty() ? name : rel + "/" + name;
    if (excluded(r, name)) continue;
    const fs::path to = dst / name;
    const auto st = e.symlink_status(ec);
    if (ec) {
      warn("stat " + e.path().string() + ": " + ec.message());
      continue;
    }
    if (fs::is_symlink(st)) {
      const fs::path target = fs::read_symlink(e.path(), ec);
      if (!ec) fs::create_symlink(target, to, ec);
    } else if (fs::is_directory(st)) {
      copy_tree(e.path(), to, r);
      fs::permissions(to, st.permissions(), ec);
      if (!ec) fs::last_write_time(to, fs::last_write_time(e.path(), ec), ec);
    } else if (fs::is_regular_file(st)) {
      fs::copy_file(e.path(), to, fs::copy_options::overwrite_existing, ec);
      if (!ec) fs::permissions(to, st.permissions(), ec);
      if (!ec) fs::last_write_time(to, fs::last_write_time(e.path(), ec), ec);
    }
    if (ec) warn("copy " + e.path().string() + ": " + ec.message());
  }
}

volatile sig_atomic_t g_signal = 0;
void on_signal(int sig) { g_signal = sig; }

// A shell command run like `cmd >/dev/null 2>&1`: true on exit 0; a program that cannot
// start fails the check.
bool quiet(const std::vector<std::string>& argv) {
  try {
    return run_inherit(argv, true, true) == 0;
  } catch (const std::exception&) {
    return false;
  }
}

bool regular_file(const fs::path& p) {  // `test -f`
  std::error_code ec;
  return fs::is_regular_file(p, ec);
}

}  // namespace

int cmd_selftest(const std::vector<std::string>& args) {
  for (const auto& a : args) {
    if (a == "-h" || a == "--help") {
      std::cout << SELFTEST_USAGE;
      return 0;
    }
  }  // other arguments are ignored, as before
  const fs::path src = find_workspace();

  std::string tmpdir = "/tmp";
  if (const char* t = std::getenv("TMPDIR"); t && *t) tmpdir = t;
  while (tmpdir.size() > 1 && tmpdir.back() == '/') tmpdir.pop_back();
  std::string tmpl = tmpdir + "/tmp.XXXXXXXXXX";
  if (!mkdtemp(tmpl.data())) {
    warn("cannot create a temp folder in " + tmpdir);
    return 1;
  }
  const fs::path tmp = tmpl;
  struct Cleanup {  // the script's `trap 'rm -rf "$tmp"' EXIT`
    fs::path p;
    ~Cleanup() {
      std::error_code ec;
      fs::remove_all(p, ec);
    }
  } cleanup{tmp};
  for (int sig : {SIGINT, SIGTERM, SIGHUP}) signal(sig, on_signal);

  fs::create_directories(tmp / "home");
  copy_tree(src, tmp / "ws", "");
  for (const char* d : {"projects", "research/topics", "knowledge"}) {
    std::error_code ec;
    fs::create_directories(tmp / "ws" / d, ec);
  }
  std::error_code ec;
  fs::current_path(tmp / "ws", ec);
  if (ec) {
    warn("cd " + (tmp / "ws").string() + ": " + ec.message());
    return 1;
  }
  const fs::path home = tmp / "home";
  setenv("HOME", home.c_str(), 1);
  // The copy must not resolve back to the source workspace through the environment.
  unsetenv("WS_ROOT");
  unsetenv("ORCH_WORKSPACE");

  using Check = std::pair<const char*, std::function<bool()>>;
  auto cmd = [](std::vector<std::string> argv) { return [argv] { return quiet(argv); }; };
  auto file = [](fs::path p) { return [p] { return regular_file(p); }; };
  const std::vector<Check> checks = {
      // Build ws first: the bin/ tools are wrappers around it.
      {"ws builds from source", cmd({"make", "-s", "-C", "tools/ws"})},
      {"ws runs", cmd({"bin/ws", "version"})},
      {"link-skills into Claude/Codex/Kiro", cmd({"bin/link-skills"})},
      {"Claude skill links resolve", file(home / ".claude/skills/workspace-coach/SKILL.md")},
      {"Codex skill links resolve", file(home / ".agents/skills/workspace-coach/SKILL.md")},
      {"Kiro skill links resolve", file(".kiro/skills/workspace-coach/SKILL.md")},
      {"new-project", cmd({"bin/new-project", "selftest-app"})},
      {"project has state files", file("projects/selftest-app/.ai/STATE.md")},
      {"new-research", cmd({"bin/new-research", "selftest-topic"})},
      {"research topic has scope", file("research/topics/selftest-topic/SCOPE.md")},
      {"new-agent-case", cmd({"bin/new-agent-case", "repo-auditor", "selftest-case"})},
      {"agent case has target.yaml", file("agents/repo-auditor/cases/selftest-case/target.yaml")},
      {"workspace-context resolves a target", cmd({"bin/workspace-context", "projects/selftest-app"})},
      {"root contract under 4 KB",
       [] {
         std::error_code e;
         auto n = fs::file_size("AGENTS.md", e);
         return !e && regular_file("AGENTS.md") && n < 4096;
       }},
      {"CLAUDE.md imports AGENTS.md",
       [] {
         try {
           return read_file("CLAUDE.md").find("@AGENTS.md") != std::string::npos;
         } catch (const std::exception&) {
           return false;
         }
       }},
      {"Kiro agent configs present", file(".kiro/agents/repo-auditor.md")},
      {"workspace-doctor runs", cmd({"bin/workspace-doctor"})},
      {"orch dry run builds a context packet",
       cmd({"bin/orch", "spawn", "projects/selftest-app", "--task", "selftest", "--dry-run"})},
  };

  int pass = 0, fail = 0;
  for (const auto& [name, check] : checks) {
    const bool ok = check();
    if (g_signal) {
      std::cout.flush();
      return 128 + g_signal;  // Cleanup removes the copy
    }
    std::cout << (ok ? "PASS  " : "FAIL  ") << name << "\n" << std::flush;
    (ok ? pass : fail)++;
  }
  std::cout << "\n" << pass << " passed, " << fail << " failed\n";
  return fail;
}

}  // namespace ws
