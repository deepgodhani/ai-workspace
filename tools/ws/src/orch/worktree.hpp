#pragma once
#include <optional>
#include <string>

namespace ws::orch {

struct Worktree {
  std::string path, branch, base, cwd;
};

std::optional<std::string> repo_root(const std::string& dir);
Worktree create_worktree(const std::string& workspace, const std::string& target_abs, const std::string& id);
std::string worktree_status(const std::string& path, const std::string& base);
void remove_worktree(const std::string& path, const std::string& branch, bool force, bool delete_branch);

}  // namespace ws::orch
