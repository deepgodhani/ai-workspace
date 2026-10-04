#include "orch/sandbox.hpp"

#include <cstdlib>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "util/fs.hpp"

namespace ws::orch {

namespace {
const char* SRT_PACKAGE = "@anthropic-ai/sandbox-runtime@0.0.78";

std::string home() {
  const char* h = std::getenv("HOME");
  return h ? h : "";
}

std::string tmp_dir() {
  std::string t = fs::temp_directory_path().string();
  while (t.size() > 1 && t.back() == '/') t.pop_back();
  return t;
}

struct Profile {
  std::vector<std::string> domains, state_dirs;
};

// Domains and state directories each CLI needs to run at all.
// Verified on macOS for kiro-cli 2.27.1 (ACP); Claude entries per Anthropic docs.
Profile profile_for(const std::string& cli) {
  const std::string h = home();
  if (cli == "kiro") return {{"*.amazonaws.com", "*.kiro.dev", "*.awsapps.com"}, {h + "/.kiro", h + "/Library/Application Support/kiro-cli"}};
  if (cli == "claude")
    return {{"api.anthropic.com", "*.anthropic.com", "claude.ai", "*.claude.ai", "*.claude.com"}, {h + "/.claude", h + "/.claude.json", h + "/.config/claude"}};
  return {};
}
}  // namespace

std::string resolve_tier(const std::string& tier, const std::string& cli) {
  if (tier != "auto" && tier != "none" && tier != "native" && tier != "srt") throw std::runtime_error("--sandbox must be auto, none, native or srt, got " + tier);
  if (tier == "auto") return cli == "kiro" ? "srt" : "native";
  if (tier == "native" && cli == "kiro") throw std::runtime_error("Kiro has no native OS sandbox on this machine; use --sandbox srt (or none)");
  if (tier == "srt" && cli == "codex") throw std::runtime_error("Codex runs in its own Seatbelt sandbox; nesting it inside srt is untested, so use --sandbox native");
  return tier;
}

std::vector<std::string> srt_wrapper(const std::string& cli, const std::vector<std::string>& writable, const std::string& run_dir) {
  Profile p = profile_for(cli);
  std::vector<std::string> allow_write = writable;
  allow_write.insert(allow_write.end(), p.state_dirs.begin(), p.state_dirs.end());
  allow_write.push_back("/private/tmp");
  allow_write.push_back(tmp_dir());
  nlohmann::ordered_json settings = {
      {"network", {{"allowedDomains", p.domains}, {"deniedDomains", nlohmann::json::array()}}},
      {"filesystem", {{"denyRead", {"~/.ssh", "~/.gnupg"}}, {"allowRead", nlohmann::json::array()}, {"allowWrite", allow_write}, {"denyWrite", nlohmann::json::array()}}},
  };
  std::string path = (fs::path(run_dir) / "srt-settings.json").string();
  write_file(path, settings.dump(2));
  if (const char* bin = std::getenv("ORCH_SRT_BIN"); bin && *bin) return {bin, "--settings", path};
  return {"npx", "-y", "-p", SRT_PACKAGE, "srt", "--settings", path};
}

const std::string& claude_native_settings() {
  static const std::string s = R"({"sandbox":{"enabled":true,"allowUnsandboxedCommands":false}})";
  return s;
}

}  // namespace ws::orch
