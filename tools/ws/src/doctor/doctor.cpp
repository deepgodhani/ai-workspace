// `ws doctor` — port of bin/workspace-doctor.
// Read-only health check: tools on PATH, workspace structure, reusable agents, the port
// registry, lifecycle skills, local research, and MCP servers. Always exits 0, except when
// HOME is unset (exit 1, as the script's `set -u` did).
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "commands.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws {
namespace {

const char* DOCTOR_USAGE =
    "Usage: ws doctor [--agent-readme NAME] [--check-exec PATH]...\n"
    "Read-only workspace health check: tools, structure, agents, port registry, skills,\n"
    "local research, and MCP servers.\n"
    "  --agent-readme NAME  also check agents/NAME/README.md (private checks come from bin/workspace-doctor)\n"
    "  --check-exec PATH    also check that PATH (relative to the workspace) is executable\n";

// `printf '%-12s'`.
std::string pad12(const std::string& s) { return s.size() >= 12 ? s : s + std::string(12 - s.size(), ' '); }

bool is_file(const fs::path& p) {
  std::error_code ec;
  return fs::is_regular_file(p, ec);
}

bool is_dir(const fs::path& p) {
  std::error_code ec;
  return fs::is_directory(p, ec);
}

bool is_exec(const fs::path& p) { return access(p.c_str(), X_OK) == 0; }  // `[[ -x p ]]`

void check_exec(const fs::path& ws, const std::string& rel) {
  if (is_exec(ws / rel)) std::cout << "ok      " << rel << "\n";
  else std::cout << "missing or non-executable " << rel << "\n";
}

// awk '/^[[:space:]]*-[[:space:]]+port:/ { print $3 }' | sort | uniq -d, as `$(…)` returns it.
std::string duplicate_ports(const std::string& text) {
  std::vector<std::string> values;
  std::istringstream in(text);
  std::string line;
  auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
  while (std::getline(in, line)) {
    size_t i = 0;
    while (i < line.size() && blank(line[i])) ++i;
    if (i >= line.size() || line[i] != '-') continue;
    size_t j = i + 1;
    while (j < line.size() && blank(line[j])) ++j;
    if (j == i + 1 || line.compare(j, 5, "port:") != 0) continue;
    // awk's default field splitting: runs of blanks/tabs; $3 may be empty.
    std::vector<std::string> fields;
    std::string f;
    for (char c : line) {
      if (c == ' ' || c == '\t') {
        if (!f.empty()) fields.push_back(f), f.clear();
      } else {
        f += c;
      }
    }
    if (!f.empty()) fields.push_back(f);
    values.push_back(fields.size() >= 3 ? fields[2] : "");
  }
  std::sort(values.begin(), values.end());
  std::string out;
  for (size_t k = 0; k < values.size();) {
    size_t e = k;
    while (e < values.size() && values[e] == values[k]) ++e;
    if (e - k > 1) out += values[k] + "\n";
    k = e;
  }
  while (!out.empty() && out.back() == '\n') out.pop_back();
  return out;
}

// Run a child with the terminal; a program that cannot start is reported like a shell would.
int passthrough(const std::vector<std::string>& argv, bool discard_stderr) {
  std::cout.flush();
  try {
    return run_inherit(argv, discard_stderr);
  } catch (const std::exception& e) {
    if (!discard_stderr) std::cerr << "ws doctor: " << e.what() << "\n";
    return 127;
  }
}

}  // namespace

int cmd_doctor(const std::vector<std::string>& args) {
  std::vector<std::pair<std::string, std::string>> extra;  // (kind, value), in argument order
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "-h" || a == "--help") {
      std::cout << DOCTOR_USAGE;
      return 0;
    }
    if (a == "--agent-readme" || a == "--check-exec") {
      if (i + 1 >= args.size()) {
        std::cerr << DOCTOR_USAGE;
        return 2;
      }
      extra.emplace_back(a, args[++i]);
    }
    // Other arguments are ignored, as the script ignored all of them.
  }

  const fs::path ws = find_workspace();
  std::cout << "Workspace: " << ws.string() << "\n\n";

  for (const char* cmd : {"git", "python3", "node", "npx", "codex", "claude", "gemini", "uv", "docker", "podman"}) {
    if (auto p = find_on_path(cmd)) std::cout << "ok      " << pad12(cmd) << " " << *p << "\n";
    else std::cout << "missing " << pad12(cmd) << "\n";
  }

  std::cout << "\nStructure:\n";
  for (const char* dir : {"_shared", "agents", "bin", "knowledge", "research", "projects", "tools", "worktrees"}) {
    std::cout << (is_dir(ws / dir) ? "ok      " : "missing ") << dir << "/\n";
  }

  std::cout << "\nReusable agents:\n";
  {
    std::vector<std::string> names;  // `agents/*/`: visible folders and links to folders
    std::error_code ec;
    if (is_dir(ws / "agents")) {
      for (const auto& e : fs::directory_iterator(ws / "agents", ec)) {
        const std::string n = e.path().filename().string();
        if (!n.empty() && n[0] != '.' && is_dir(e.path())) names.push_back(n);
      }
    }
    std::sort(names.begin(), names.end());
    for (const auto& n : names) {
      std::cout << (is_file(ws / "agents" / n / "AGENTS.md") ? "ok      " : "missing ") << "agents/" << n << "/AGENTS.md\n";
    }
  }
  check_exec(ws, "bin/new-agent-case");
  for (const auto& [kind, value] : extra) {
    if (kind == "--check-exec") {
      check_exec(ws, value);
    } else if (is_file(ws / "agents" / value / "README.md")) {
      std::cout << "ok      agents/" << value << "/\n";
    } else {
      std::cout << "missing agents/" << value << "/README.md\n";
    }
  }
  const fs::path registry = ws / "tools" / "database-port-registry.yaml";
  if (is_file(registry)) {
    std::cout << "ok      tools/database-port-registry.yaml\n";
    const std::string dups = duplicate_ports(read_file(registry));
    if (!dups.empty()) std::cout << "warning duplicate registered database port(s): " << dups << "\n";
    else std::cout << "ok      database port assignments are unique\n";
  } else {
    std::cout << "missing tools/database-port-registry.yaml\n";
  }

  std::cout << "\nWorkspace lifecycle skills:\n";
  for (const char* skill : {"workspace-coach", "handoff", "resume-work", "daily-summary", "temp-session"}) {
    const std::string rel = std::string("_shared/skills/") + skill + "/SKILL.md";
    std::cout << (is_file(ws / rel) ? "ok      " : "missing ") << rel << "\n";
  }
  check_exec(ws, "bin/workspace-context");
  check_exec(ws, "bin/init-ai-workspace.sh");

  std::cout << "\nLocal research:\n";
  const fs::path status = ws / "tools/local-research/bin/status";
  if (is_exec(status)) passthrough({status.string()}, false);  // its exit status is ignored
  else std::cout << "missing tools/local-research/bin/status\n";

  const char* home = std::getenv("HOME");
  if (!home) {
    std::cout.flush();
    std::cerr << "ws doctor: HOME is not set\n";
    return 1;
  }
  for (const std::string& f : {(ws / "_shared/skills/local-web-research/SKILL.md").string(),
                               std::string(home) + "/.agents/skills/local-web-research/SKILL.md",
                               std::string(home) + "/.claude/skills/local-web-research/SKILL.md"}) {
    std::cout << (is_file(f) ? "ok      " : "missing ") << f << "\n";
  }

  std::cout << "\nMCP status:\n";
  std::cout << "Firecrawl is optional; local-web-research is the default.\n";
  for (const char* cli : {"codex", "claude"}) {
    if (auto p = find_on_path(cli)) {
      if (passthrough({*p, "mcp", "list"}, true) != 0) std::cout << cli << ": unable to list MCP servers\n";
    } else {
      std::cout << cli << ": not installed\n";
    }
  }
  return 0;
}

}  // namespace ws
