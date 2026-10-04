#pragma once
#include <string>
#include <vector>

namespace ws::orch {

// "auto" resolves per CLI: Claude/Codex → native, Kiro → srt.
std::string resolve_tier(const std::string& tier, const std::string& cli);
// Writes <run_dir>/srt-settings.json and returns the argv prefix that wraps a command in srt.
std::vector<std::string> srt_wrapper(const std::string& cli, const std::vector<std::string>& writable, const std::string& run_dir);
// Claude Code's built-in sandbox: Bash may write only in the working directory; no unsandboxed fallback.
const std::string& claude_native_settings();

}  // namespace ws::orch
