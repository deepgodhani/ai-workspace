#pragma once
#include <optional>
#include <string>
#include <vector>

#include "util/fs.hpp"

namespace ws {

// Repositories that live outside the workspace but that the workspace controls.
// Listed in <workspace>/external-projects.txt, one per line: `<name> <path>`;
// `#` starts a comment and a path may begin with `~/`. Entries whose path does
// not exist are skipped, so one machine's list can name repos another lacks.
inline constexpr const char* EXTERNAL_PROJECTS_FILE = "external-projects.txt";

struct ExternalProject {
  std::string name;
  fs::path root;  // canonical
};

// Throws on a malformed line or an entry that is too broad: `/`, $HOME, the
// workspace itself, an ancestor of it, or a folder already inside it.
std::vector<ExternalProject> load_external_projects(const fs::path& workspace);

struct ExternalMatch {
  ExternalProject project;
  std::string label;  // "@name" or "@name/sub/dir" — how the target is shown and recorded
};

// The registered project containing the canonical path `abs`, if any.
std::optional<ExternalMatch> match_external(const fs::path& workspace, const fs::path& abs);

// Resolve "name", "@name", "name/sub" or "@name/sub" to a path under a registered root.
std::optional<fs::path> lookup_external(const fs::path& workspace, const std::string& input);

}  // namespace ws
