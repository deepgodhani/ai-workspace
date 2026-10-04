#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ws::orch {

using json = nlohmann::json;

constexpr size_t PER_FILE_CHARS = 4000;
constexpr size_t PACKET_FILE_BUDGET = 14000;

const json& result_schema();
bool is_result(const json& v);
std::optional<json> parse_result(const std::string& text);
std::string render_result(const std::string& id, const json& r);

struct PacketInput {
  std::string task, target_rel, target_abs, cwd, mode, branch;
};
std::string build_packet(const PacketInput& p);

struct MergeSource {
  std::string id, cli, mode, target;
  json result;
  std::optional<std::string> worktree, branch;
};
std::string build_merge_message(const std::vector<MergeSource>& sources, const std::optional<std::string>& task);

}  // namespace ws::orch
