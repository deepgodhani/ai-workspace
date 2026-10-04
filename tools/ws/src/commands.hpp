#pragma once
#include <string>
#include <vector>

namespace ws {
int cmd_context(const std::vector<std::string>& args);
int cmd_new(const std::vector<std::string>& args);
int cmd_orch(const std::vector<std::string>& args);
int cmd_version(const std::vector<std::string>& args);
}  // namespace ws
