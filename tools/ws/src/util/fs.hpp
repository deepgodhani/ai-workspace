#pragma once
#include <filesystem>
#include <string>

namespace ws {
namespace fs = std::filesystem;

std::string read_file(const fs::path& p);
void write_file(const fs::path& p, const std::string& content);
std::string read_stdin();

// Workspace root: $ORCH_WORKSPACE / $WS_ROOT, else the nearest ancestor of `start`
// that holds both AGENTS.md and _shared/.
fs::path find_workspace(const fs::path& start = fs::current_path());

// True when `child` is `parent` or lies inside it (both canonicalised lexically).
bool is_within(const fs::path& parent, const fs::path& child);

std::string trim(const std::string& s);
std::string random_hex(size_t n);
std::string uuid_v4();
std::string now_iso();

}  // namespace ws
