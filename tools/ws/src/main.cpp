#include <signal.h>

#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "commands.hpp"

namespace {

const char* USAGE = R"(ws — AI Workspace command-line tool

  ws orch <command> …      scoped AI CLI child sessions (spawn, resume, merge, list, show, diff, close)
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
      {"orch", ws::cmd_orch},
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
