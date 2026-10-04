// `ws link-skills` — port of bin/link-skills.
// Links the workspace's shared skills into each AI CLI's skill directory. Idempotent;
// never replaces an existing entry that points elsewhere.
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "commands.hpp"
#include "util/fs.hpp"

namespace ws {
namespace {

const char* LINK_SKILLS_USAGE =
    "Usage: ws link-skills\n"
    "Links _shared/skills/* into ~/.claude/skills, ~/.agents/skills (absolute links)\n"
    "and <workspace>/.kiro/skills (relative links). Existing entries are never replaced.\n";

// Skill folders in byte order, as the shell glob `*/` saw them: directories (or links to
// directories) whose name does not start with a dot.
std::vector<std::string> skill_dirs(const fs::path& root) {
  std::vector<std::string> names;
  std::error_code ec;
  if (!fs::is_directory(root, ec)) return names;
  for (const auto& e : fs::directory_iterator(root)) {
    const std::string name = e.path().filename().string();
    if (!name.empty() && name[0] != '.' && fs::is_directory(e.path(), ec)) names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

// True when `p` exists or is a symlink (even a dangling one): `[[ -e p || -L p ]]`.
bool occupied(const fs::path& p) {
  std::error_code ec;
  return fs::exists(fs::symlink_status(p, ec));
}

void link_into(const fs::path& src_root, const std::vector<std::string>& skills, const fs::path& dest_root,
               const std::string& label) {
  fs::create_directories(dest_root);
  for (const auto& name : skills) {
    std::error_code ec;
    if (!fs::is_regular_file(src_root / name / "SKILL.md", ec)) continue;
    const fs::path src = src_root / name;
    const fs::path dest = dest_root / name;
    if (fs::is_symlink(fs::symlink_status(dest, ec)) && fs::read_symlink(dest, ec) == src) {
      std::cout << "ok     " << label << "/" << name << "\n";
    } else if (occupied(dest)) {
      std::cout << "skip   " << label << "/" << name << " (exists, points elsewhere)\n";
    } else {
      fs::create_directory_symlink(src, dest);
      std::cout << "link   " << label << "/" << name << "\n";
    }
  }
}

}  // namespace

int cmd_link_skills(const std::vector<std::string>& args) {
  // The script ignored its arguments; keep that, except for help.
  for (const auto& a : args) {
    if (a == "-h" || a == "--help") {
      std::cout << LINK_SKILLS_USAGE;
      return 0;
    }
  }
  const char* home_env = std::getenv("HOME");
  if (!home_env || !*home_env) {
    std::cerr << "link-skills: HOME is not set\n";
    return 1;
  }
  const fs::path home = home_env;
  const fs::path workspace = find_workspace();
  const fs::path src_root = workspace / "_shared" / "skills";
  const std::vector<std::string> skills = skill_dirs(src_root);

  try {
    link_into(src_root, skills, home / ".claude/skills", "~/.claude/skills");  // Claude Code
    link_into(src_root, skills, home / ".agents/skills", "~/.agents/skills");  // Codex and AGENTS-style CLIs

    // Kiro reads workspace-level skills; relative links keep the repo portable. Like the
    // script, every skill folder is linked here (no SKILL.md check) and existing entries are
    // left alone silently.
    const fs::path kiro = workspace / ".kiro" / "skills";
    fs::create_directories(kiro);
    for (const auto& name : skills) {
      const fs::path dest = kiro / name;
      if (occupied(dest)) continue;
      fs::create_directory_symlink(fs::path("../../_shared/skills") / name, dest);
      std::cout << "link   .kiro/skills/" << name << "\n";
    }
  } catch (const fs::filesystem_error& e) {
    std::cout.flush();
    std::cerr << "link-skills: " << e.what() << "\n";
    return 1;
  }
  return 0;
}

}  // namespace ws
