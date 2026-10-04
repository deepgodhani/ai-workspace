#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ws::orch {

struct SpawnOpts {
  std::string mode;  // "read" | "write"
  std::string prompt;
  std::string cwd;
  std::optional<std::string> session_id, resume_id, model;
  std::optional<double> budget_usd;
  std::vector<std::string> allow;
  bool full = false;
  std::string sandbox = "none";
  std::string schema_file, last_message_file;
};

struct Invocation {
  std::vector<std::string> argv;
  std::string input;
};

struct ChildOutput {
  std::optional<std::string> cli_session_id;
  std::optional<nlohmann::json> result;
  std::optional<double> cost_usd;
  long long input_tokens = 0, output_tokens = 0, cache_read_tokens = 0;
  std::optional<std::string> error;
  double credits = 0;
};

Invocation claude_invocation(const SpawnOpts& o);
Invocation codex_invocation(const SpawnOpts& o);
ChildOutput parse_claude(const std::string& stdout_text);
ChildOutput parse_codex(const std::string& stdout_text, const std::string& last_message);
std::string format_number(double v);

}  // namespace ws::orch
