#include <signal.h>

#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "commands.hpp"

namespace {

const char* USAGE = R"(ws — AI Workspace command-line tool

  ws context <target>      target type, Git state, instructions, startup context sizes
  ws doctor                read-only health check of tools, structure, skills, and MCP servers
  ws export-oss [dest]     stage, scan, and sync the publishable framework (--check: scan only)
  ws firecrawl configure|mcp  save the optional Firecrawl key / run its MCP server
  ws link-skills           link _shared/skills into Claude Code, Codex, and Kiro
  ws new <kind> …          scaffold a project, research topic, or agent case from its template
  ws orch <command> …      scoped AI CLI child sessions (spawn, resume, merge, list, show, diff, close)
  ws selftest              end-to-end check of the framework on a throwaway copy
  ws session-report        does context per message grow with session length? (tokscale)
  ws usage                 local token and cost summary across AI CLIs (tokscale)
  ws version

Run `ws <command> --help` for details.)";

}  // namespace

int main(int argc, char** argv) {
  signal(SIGPIPE, SIG_IGN);
  std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty() || args[0] == "-h" || args[0] == "--help" || args[0] == "help") {
    std::cout << USAGE << "\n";
    return 0;
  }
  const std::map<std::string, int (*)(const std::vector<std::string>&)> commands = {
      {"context", ws::cmd_context},
      {"doctor", ws::cmd_doctor},
      {"export-oss", ws::cmd_export_oss},
      {"firecrawl", ws::cmd_firecrawl},
      {"link-skills", ws::cmd_link_skills},
      {"new", ws::cmd_new},
      {"orch", ws::cmd_orch},
      {"selftest", ws::cmd_selftest},
      {"session-report", ws::cmd_session_report},
      {"usage", ws::cmd_usage},
      {"version", ws::cmd_version},
  };
  auto it = commands.find(args[0]);
  if (it == commands.end()) {
    std::cerr << "ws: unknown command '" << args[0] << "'\n\n" << USAGE << "\n";
    return 2;
  }
  try {
    return it->second(std::vector<std::string>(args.begin() + 1, args.end()));
  } catch (const std::exception& e) {
    std::cerr << "ws " << args[0] << ": " << e.what() << "\n";
    return 2;
  }
}
