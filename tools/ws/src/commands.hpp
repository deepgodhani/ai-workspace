#pragma once
#include <string>
#include <vector>

namespace ws {
int cmd_context(const std::vector<std::string>& args);
int cmd_doctor(const std::vector<std::string>& args);
int cmd_export_oss(const std::vector<std::string>& args);
int cmd_firecrawl(const std::vector<std::string>& args);
int cmd_link_skills(const std::vector<std::string>& args);
int cmd_new(const std::vector<std::string>& args);
int cmd_orch(const std::vector<std::string>& args);
int cmd_selftest(const std::vector<std::string>& args);
int cmd_session_report(const std::vector<std::string>& args);
int cmd_usage(const std::vector<std::string>& args);
int cmd_version(const std::vector<std::string>& args);
}  // namespace ws
