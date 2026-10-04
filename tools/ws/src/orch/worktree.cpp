#include "orch/worktree.hpp"

#include <stdexcept>

#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws::orch {

std::optional<std::string> repo_root(const std::string& dir) {
  auto r = run({"git", "rev-parse", "--show-toplevel"}, RunOptions{dir, "", 0, {}});
  if (r.code != 0) return std::nullopt;
  return trim(r.out);
}

Worktree create_worktree(const std::string& workspace, const std::string& target_abs, const std::string& id) {
  auto root = repo_root(target_abs);
  if (!root) throw std::runtime_error("--write needs the target inside a Git repository: " + target_abs);
  fs::path root_c = fs::canonical(*root), target_c = fs::canonical(target_abs);
  std::string base = run_ok({"git", "rev-parse", "HEAD"}, root_c.string());
  std::string branch = "orch/" + id;
  fs::path path = fs::path(workspace) / "worktrees" / (root_c.filename().string() + "-" + id);
  if (fs::exists(path)) throw std::runtime_error("worktree path already exists: " + path.string());
  run_ok({"git", "worktree", "add", "-q", "-b", branch, path.string(), base}, root_c.string());
  fs::path rel = target_c.lexically_relative(root_c);
  fs::path cwd = (rel.empty() || rel == ".") ? path : path / rel;
  return {path.string(), branch, base, cwd.string()};
}

std::string worktree_status(const std::string& path, const std::string& base) {
  std::string status = run_ok({"git", "status", "--short"}, path);
  std::string committed = run_ok({"git", "diff", "--stat", base + "..HEAD"}, path);
  std::string working = run_ok({"git", "diff", "--stat"}, path);
  auto or_none = [](const std::string& s) { return s.empty() ? std::string("(none)") : s; };
  return "uncommitted:\n" + or_none(status) + "\n\ncommitted since base:\n" + or_none(committed) + "\n\nworking-tree diff:\n" + or_none(working);
}

void remove_worktree(const std::string& path, const std::string& branch, bool force, bool delete_branch) {
  if (!fs::exists(path)) return;
  bool dirty = !run_ok({"git", "status", "--porcelain"}, path).empty();
  if (dirty && !force) throw std::runtime_error("worktree has uncommitted changes; review with 'orch diff', then rerun with --force to discard: " + path);
  fs::path common = run_ok({"git", "rev-parse", "--git-common-dir"}, path);
  fs::path main_repo = ((common.is_absolute() ? common : fs::path(path) / common) / "..").lexically_normal();
  std::vector<std::string> cmd = {"git", "worktree", "remove"};
  if (force) cmd.push_back("--force");
  cmd.push_back(path);
  run_ok(cmd, main_repo.string());
  if (delete_branch) run_ok({"git", "branch", "-D", branch}, main_repo.string());
}

}  // namespace ws::orch
