#include "util/external.hpp"

#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace ws {

namespace {

std::string where(size_t line) { return std::string(EXTERNAL_PROJECTS_FILE) + " line " + std::to_string(line) + ": "; }

fs::path expand_home(const std::string& p) {
  if (p == "~" || p.rfind("~/", 0) == 0) {
    const char* h = std::getenv("HOME");
    if (!h || !*h) throw std::runtime_error("HOME is not set; cannot expand " + p);
    return fs::path(h) / p.substr(p == "~" ? 1 : 2);
  }
  return fs::path(p);
}

}  // namespace

std::vector<ExternalProject> load_external_projects(const fs::path& workspace) {
  std::vector<ExternalProject> out;
  std::ifstream in(workspace / EXTERNAL_PROJECTS_FILE);
  if (!in) return out;

  static const std::regex name_re("^[A-Za-z0-9][A-Za-z0-9._-]*$");
  std::error_code ec;
  fs::path home;
  if (const char* h = std::getenv("HOME"); h && *h) home = fs::weakly_canonical(h, ec);

  std::string raw;
  size_t n = 0;
  while (std::getline(in, raw)) {
    ++n;
    std::string line = raw.substr(0, raw.find('#'));
    std::istringstream words(line);
    std::string name, path, extra;
    if (!(words >> name)) continue;  // blank or comment
    if (!(words >> path)) throw std::runtime_error(where(n) + "expected `<name> <path>`");
    if (words >> extra) throw std::runtime_error(where(n) + "expected `<name> <path>` (paths with spaces are not supported)");
    if (!std::regex_match(name, name_re)) throw std::runtime_error(where(n) + "invalid name '" + name + "'");
    for (const auto& p : out)
      if (p.name == name) throw std::runtime_error(where(n) + "duplicate name '" + name + "'");

    fs::path p = expand_home(path);
    if (!p.is_absolute()) throw std::runtime_error(where(n) + "path must be absolute or start with ~/: " + path);
    if (!fs::is_directory(p, ec)) continue;  // not on this machine
    fs::path root = fs::canonical(p);
    if (root == root.root_path()) throw std::runtime_error(where(n) + "refusing the filesystem root");
    if (!home.empty() && root == home) throw std::runtime_error(where(n) + "refusing the home directory itself; list the repository folder");
    if (root == workspace || is_within(root, workspace))
      throw std::runtime_error(where(n) + "refusing a folder that contains the workspace: " + root.string());
    if (is_within(workspace, root))
      throw std::runtime_error(where(n) + "already inside the workspace; target it directly: " + root.string());
    out.push_back({name, root});
  }
  return out;
}

std::optional<ExternalMatch> match_external(const fs::path& workspace, const fs::path& abs) {
  for (const auto& p : load_external_projects(workspace)) {
    if (abs == p.root) return ExternalMatch{p, "@" + p.name};
    if (is_within(p.root, abs)) return ExternalMatch{p, "@" + p.name + "/" + abs.lexically_relative(p.root).string()};
  }
  return std::nullopt;
}

std::optional<fs::path> lookup_external(const fs::path& workspace, const std::string& input) {
  std::string s = input;
  if (!s.empty() && s.front() == '@') s.erase(0, 1);
  while (!s.empty() && s.back() == '/') s.pop_back();
  if (s.empty() || s.front() == '/') return std::nullopt;
  const auto slash = s.find('/');
  const std::string name = s.substr(0, slash);
  for (const auto& p : load_external_projects(workspace)) {
    if (p.name != name) continue;
    fs::path candidate = slash == std::string::npos ? p.root : p.root / s.substr(slash + 1);
    std::error_code ec;
    if (!fs::exists(candidate, ec)) return std::nullopt;
    return candidate;
  }
  return std::nullopt;
}

}  // namespace ws
