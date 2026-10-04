#pragma once
#include <optional>
#include <string>
#include <vector>

#include "orch/adapters.hpp"

namespace ws::orch {

struct PermissionRequest {
  std::optional<std::string> kind;
  std::string title;
  std::optional<std::string> command;
  std::vector<std::string> paths;
};

struct Decision {
  bool allow;
  std::string reason;
};

// `Bash(prefix:*)` matches the prefix as a whole word; anything else must match exactly.
bool command_allowed(const std::string& command, const std::vector<std::string>& allow);
// Read mode rejects everything. Write mode allows edit/delete/move only inside cwd,
// and execute only for commands matching --allow. Everything else is rejected.
Decision decide(const PermissionRequest& req, const std::string& mode, const std::string& cwd, const std::vector<std::string>& allow);

struct AcpRun {
  std::string cwd, mode, prompt, log_file;
  std::optional<std::string> resume_id, model;
  std::vector<std::string> allow, wrapper;
  int timeout_ms = 0;
};

struct AcpOutput : ChildOutput {
  std::vector<std::string> denied;
  bool timed_out = false;
};

extern const char* JSON_INSTRUCTION_PREFIX;
std::string json_instruction();
std::vector<std::string> acp_command(const std::optional<std::string>& model);
AcpOutput run_acp(const AcpRun& o);

}  // namespace ws::orch
