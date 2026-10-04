// `ws context <target>` — port of bin/workspace-context.
// Resolves a workspace target and prints its type, Git state, instruction files, and
// startup context sizes without loading file contents.
#include <fnmatch.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "commands.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws {
namespace {

const char* CONTEXT_USAGE =
    "Usage: ws context <target>\n"
    "Examples: projects/my-project, my-project, research/topics/mongodb\n";

constexpr std::uintmax_t CONTEXT_WARN_BYTES = 12000;

// Shell `case` glob semantics: `*` also matches `/`.
bool glob(const char* pattern, const std::string& s) { return fnmatch(pattern, s.c_str(), 0) == 0; }

bool starts_with(const std::string& s, const std::string& prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

// `${s#prefix}`: drop `prefix` when present, else return `s` unchanged.
std::string strip_prefix(const std::string& s, const std::string& prefix) {
  return starts_with(s, prefix) ? s.substr(prefix.size()) : s;
}

std::string target_type(const std::string& relative) {
  const std::string r = relative + "/";
  static const std::vector<std::pair<const char*, const char*>> rules = {
      {"projects/*", "project"},          {"agents/*/cases/*", "agent-case"}, {"agents/*", "agent"},
      {"research/topics/*", "research"},  {"knowledge/*", "knowledge"},       {"tools/*", "tool"},
      {"_shared/*", "shared"},            {"temp-sessions/*", "temporary"},   {"worktrees/*", "worktree"},
      {"./*", "workspace"},
  };
  for (const auto& [pattern, type] : rules)
    if (glob(pattern, r)) return type;
  return "workspace-area";
}

size_t count_newlines(const std::string& s) { return static_cast<size_t>(std::count(s.begin(), s.end(), '\n')); }

}  // namespace

int cmd_context(const std::vector<std::string>& args) {
  if (args.size() == 1 && (args[0] == "-h" || args[0] == "--help")) {
    std::cout << CONTEXT_USAGE;
    return 0;
  }
  if (args.size() != 1) {
    std::cerr << CONTEXT_USAGE;
    return 2;
  }
  std::string requested = args[0];
  if (!requested.empty() && requested.back() == '/') requested.pop_back();
  if (requested.empty()) {
    std::cerr << CONTEXT_USAGE;
    return 2;
  }

  const fs::path workspace = find_workspace();
  const std::string ws_str = workspace.string();
  std::error_code ec;

  // Resolve: absolute path, else workspace-relative path, else a bare name in the known areas.
  std::vector<std::string> matches;
  if (requested.front() == '/') {
    if (fs::is_directory(requested, ec)) matches.push_back(requested);
  } else if (fs::is_directory(workspace / requested, ec)) {
    matches.push_back(ws_str + "/" + requested);
  } else {
    for (const char* area : {"projects", "agents", "research/topics", "knowledge", "tools", "temp-sessions", "worktrees"}) {
      std::string candidate = ws_str + "/" + area + "/" + requested;
      if (fs::is_directory(candidate, ec)) matches.push_back(candidate);
    }
  }
  if (matches.empty()) {
    std::cerr << "Target not found: " << requested << "\n";
    return 1;
  }
  if (matches.size() > 1) {
    std::cerr << "Ambiguous target: " << requested << "\n";
    for (const auto& m : matches) std::cerr << "  " << m << "\n";
    return 1;
  }

  const std::string target = fs::canonical(matches[0]).string();
  if (!starts_with(target + "/", ws_str + "/")) {
    std::cerr << "Target is outside the workspace: " << target << "\n";
    return 1;
  }
  const std::string prefix = ws_str + "/";
  const std::string relative = target == ws_str ? "." : strip_prefix(target, prefix);
  const std::string type = target_type(relative);

  // Git state of the owning repository, if any.
  std::string git_relative = "none", branch = "none", changed = "0";
  RunOptions in_target;
  in_target.cwd = target;
  RunResult top;
  try {
    top = run({"git", "-C", target, "rev-parse", "--show-toplevel"}, in_target);
  } catch (const std::runtime_error&) {
    top.code = -1;  // git not installed: report no repository, as the script did
  }
  if (top.code == 0) {
    std::string git_root = top.out;
    if (!git_root.empty() && git_root.back() == '\n') git_root.pop_back();
    git_relative = strip_prefix(git_root, prefix);
    RunResult br = run({"git", "-C", git_root, "branch", "--show-current"}, in_target);
    branch = br.code == 0 ? trim(br.out) : "";
    if (branch.empty()) branch = "detached";
    RunResult st = run({"git", "-C", git_root, "status", "--short"}, in_target);
    changed = std::to_string(count_newlines(st.out));
  }

  std::cout << "workspace: " << ws_str << "\n"
            << "target: " << relative << "\n"
            << "type: " << type << "\n"
            << "write_scope: " << relative << "\n"
            << "git_root: " << git_relative << "\n"
            << "git_branch: " << branch << "\n"
            << "git_changed_paths: " << changed << "\n";

  // AGENTS.md files from the workspace root down to the target.
  std::cout << "instructions:\n";
  std::vector<std::string> instructions;
  for (fs::path current = target;; current = current.parent_path()) {
    if (fs::is_regular_file(current / "AGENTS.md", ec)) instructions.push_back((current / "AGENTS.md").string());
    if (current.string() == ws_str || current == current.root_path()) break;
  }
  for (auto it = instructions.rbegin(); it != instructions.rend(); ++it)
    std::cout << "  - " << strip_prefix(*it, prefix) << "\n";

  // Startup files that should be read before work begins.
  std::vector<std::string> required;
  auto add_required = [&](const std::string& path) {
    if (fs::is_regular_file(path, ec)) required.push_back(path);
  };
  for (const char* f : {"/.ai/STATE.md", "/.ai/TASKS.md", "/.ai/INDEX.md"}) add_required(target + f);
  if (type == "research") {
    for (const char* f : {"/SCOPE.md", "/STATE.md", "/CONTEXT.md", "/GAPS.md"}) add_required(target + f);
  } else if (type == "agent-case") {
    const std::string agent_root = target.substr(0, target.find("/cases/"));  // `${target%%/cases/*}`
    for (const char* f : {"/.ai/STATE.md", "/.ai/TASKS.md", "/.ai/INDEX.md"}) add_required(agent_root + f);
    add_required(target + "/STATUS.md");
    add_required(target + "/verification.yaml");
  }

  std::cout << "required_context:\n";
  std::uintmax_t total = 0;
  if (required.empty()) std::cout << "  - none\n";
  for (const auto& path : required) {
    std::uintmax_t bytes = fs::file_size(path, ec);
    if (ec) bytes = 0;
    total += bytes;
    std::cout << "  - " << strip_prefix(path, prefix) << " (" << bytes << " bytes)\n";
  }
  std::cout << "required_context_bytes: " << total << "\n"
            << "context_warning: " << (total > CONTEXT_WARN_BYTES ? "compact startup files before loading all of them" : "none")
            << "\n";

  // Larger material to open only for a specific need.
  std::cout << "conditional_context:\n";
  for (const char* name : {".ai/DECISIONS.md", ".ai/sessions", "docs", "README.md", "REPORT.md", "FINDINGS.md",
                           "QUERY-RESPONSES.md", "behavior-catalog.yaml", "invariants.yaml", "SOURCES.md", "raw", "inputs",
                           ".runtime"}) {
    if (fs::exists(fs::path(target) / name, ec)) std::cout << "  - " << relative << "/" << name << "\n";
  }
  return 0;
}

}  // namespace ws
