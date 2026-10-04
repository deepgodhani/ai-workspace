// `ws new project|research|agent-case` — ports of bin/new-project, bin/new-research,
// bin/new-agent-case. Each copies a template into a fresh folder and fills placeholders.
#include <algorithm>
#include <ctime>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include "commands.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws {
namespace {

const char* NEW_USAGE = R"(Usage: ws new <kind> …

  ws new project <project-name>
  ws new research <topic-name> [root-url]
  ws new agent-case <agent-name> <case-name>
)";

// Like `cp -a src/. dst/`: recursive, keeps file and directory modes and symlinks.
void copy_tree(const fs::path& src, const fs::path& dst) {
  fs::create_directories(dst);
  fs::permissions(dst, fs::status(src).permissions());
  for (const auto& e : fs::recursive_directory_iterator(src)) {
    const fs::path to = dst / fs::relative(e.path(), src);
    const auto st = e.symlink_status();
    if (fs::is_symlink(st)) {
      fs::copy_symlink(e.path(), to);
    } else if (fs::is_directory(st)) {
      fs::create_directory(to);
      fs::permissions(to, st.permissions());
    } else {
      fs::copy_file(e.path(), to);  // copies permission bits
      fs::permissions(to, st.permissions());
    }
  }
}

// Regular files (not symlinks) under `root`, collected before any change is made.
std::vector<fs::path> regular_files(const fs::path& root) {
  std::vector<fs::path> out;
  for (const auto& e : fs::recursive_directory_iterator(root))
    if (fs::is_regular_file(e.symlink_status())) out.push_back(e.path());
  std::sort(out.begin(), out.end());
  return out;
}

std::string replace_all(std::string s, const std::string& from, const std::string& to) {
  for (size_t i = s.find(from); i != std::string::npos; i = s.find(from, i + to.size())) s.replace(i, from.size(), to);
  return s;
}

// Strict UTF-8 (as Python's decoder: no overlongs, surrogates, or code points above U+10FFFF).
bool valid_utf8(const std::string& s) {
  const auto at = [&](size_t i) { return static_cast<unsigned char>(s[i]); };
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = at(i);
    size_t n;
    unsigned char lo = 0x80, hi = 0xBF;  // allowed range of the second byte
    if (c < 0x80) n = 0;
    else if (c >= 0xC2 && c <= 0xDF) n = 1;
    else if (c >= 0xE0 && c <= 0xEF) n = 2, lo = c == 0xE0 ? 0xA0 : 0x80, hi = c == 0xED ? 0x9F : 0xBF;
    else if (c >= 0xF0 && c <= 0xF4) n = 3, lo = c == 0xF0 ? 0x90 : 0x80, hi = c == 0xF4 ? 0x8F : 0xBF;
    else return false;
    if (i + n >= s.size() && n > 0) return false;
    for (size_t k = 1; k <= n; ++k) {
      const unsigned char b = at(i + k);
      if (k == 1 ? (b < lo || b > hi) : (b & 0xC0) != 0x80) return false;
    }
    i += n + 1;
  }
  return true;
}

// Rewrite `p` in place (keeping its mode) when the substitution changes it.
void substitute(const fs::path& p, const std::vector<std::pair<std::string, std::string>>& pairs) {
  const std::string before = read_file(p);
  std::string after = before;
  for (const auto& [from, to] : pairs) after = replace_all(after, from, to);
  if (after != before) {
    const auto mode = fs::status(p).permissions();
    write_file(p, after);
    fs::permissions(p, mode);
  }
}

std::string today() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
  localtime_r(&t, &tm);
  char buf[16];
  std::strftime(buf, sizeof buf, "%Y-%m-%d", &tm);
  return buf;
}

bool taken(const fs::path& p) {
  std::error_code ec;
  return fs::exists(fs::symlink_status(p, ec));
}

int new_project(const std::vector<std::string>& args) {
  if (args.size() != 1) {
    std::cerr << "Usage: ws new project <project-name>\n";
    return 2;
  }
  const std::string& name = args[0];
  if (!std::regex_match(name, std::regex("[A-Za-z0-9][A-Za-z0-9._-]*"))) {
    std::cerr << "Use letters, numbers, dots, underscores, or hyphens; start with a letter or number.\n";
    return 2;
  }
  const fs::path workspace = find_workspace();
  const fs::path target = workspace / "projects" / name;
  const fs::path tmpl = workspace / "_shared/templates/project";
  if (taken(target)) {
    std::cerr << "Project already exists: " << target.string() << "\n";
    return 1;
  }
  if (!fs::is_directory(tmpl)) {
    std::cerr << "Template not found: " << tmpl.string() << "\n";
    return 1;
  }
  copy_tree(tmpl, target);
  if (fs::is_regular_file(target / "AGENTS.md")) substitute(target / "AGENTS.md", {{"{{PROJECT_NAME}}", name}});
  write_file(target / "README.md",
             "# " + name + "\n\nDescribe the project, setup, commands, architecture, and usage here.\n");
  RunOptions opts;
  opts.cwd = target.string();
  RunResult git;
  try {
    git = run({"git", "-C", target.string(), "init", "-q"}, opts);
  } catch (const std::runtime_error&) {
    git.code = 0;  // git not installed: the script skipped `git init`
  }
  if (git.code != 0) {
    std::cerr << git.err;
    return 1;
  }
  std::cout << "Created project: " << target.string() << "\n"
            << "Next from your workspace-root session:\n"
            << "  $workspace-coach activate projects/" << name << ", initialize its state, and continue\n";
  return 0;
}

// Lowercase ASCII, runs of anything else become one `-`, trimmed of leading/trailing `-`.
std::string slugify(const std::string& topic) {
  std::string slug;
  for (char ch : topic) {
    const char c = (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) slug += c;
    else if (slug.empty() || slug.back() != '-') slug += '-';
  }
  const auto b = slug.find_first_not_of('-');
  if (b == std::string::npos) return "";
  return slug.substr(b, slug.find_last_not_of('-') - b + 1);
}

int new_research(const std::vector<std::string>& args) {
  if (args.empty() || args.size() > 2) {
    std::cerr << "Usage: ws new research <topic-name> [root-url]\n";
    return 2;
  }
  const std::string& topic = args[0];
  const std::string root_url = args.size() == 2 ? args[1] : "";
  const std::string slug = slugify(topic);
  if (slug.empty()) {
    std::cerr << "Could not create a valid topic slug.\n";
    return 2;
  }
  const fs::path workspace = find_workspace();
  const fs::path target = workspace / "research/topics" / slug;
  const fs::path tmpl = workspace / "_shared/templates/research";
  if (taken(target)) {
    std::cerr << "Research topic already exists: " << target.string() << "\n";
    return 1;
  }
  if (!fs::is_directory(tmpl)) {
    std::cerr << "Template not found: " << tmpl.string() << "\n";
    return 1;
  }
  copy_tree(tmpl, target);
  const std::string date = today();
  for (const auto& p : regular_files(target)) {
    if (!valid_utf8(read_file(p))) continue;  // binary files are left alone
    substitute(p, {{"{{TOPIC}}", topic}, {"{{ROOT_URL}}", root_url}, {"{{DATE}}", date}});
  }
  std::cout << "Created research topic: " << target.string() << "\n"
            << "Next from your workspace-root session:\n"
            << "  $workspace-coach activate research/topics/" << slug << " and review its scope\n";
  return 0;
}

int new_agent_case(const std::vector<std::string>& args) {
  if (args.size() != 2) {
    std::cerr << "Usage: ws new agent-case <agent-name> <case-name>\n"
              << "Example: ws new agent-case repo-auditor my-project-audit\n";
    return 2;
  }
  const std::string& agent = args[0];
  const std::string& name = args[1];
  if (!std::regex_match(name, std::regex("[a-z0-9][a-z0-9-]*"))) {
    std::cerr << "Use lowercase letters, numbers, and hyphens for the case name; start with a letter or number.\n";
    return 2;
  }
  const fs::path workspace = find_workspace();
  const fs::path agent_dir = workspace / "agents" / agent;
  const fs::path tmpl = agent_dir / "cases/_template";
  const fs::path target = agent_dir / "cases" / name;
  if (!fs::is_directory(agent_dir)) {
    std::cerr << "Agent not found: " << agent_dir.string() << "\n";
    return 1;
  }
  if (!fs::is_directory(tmpl)) {
    std::cerr << "No case template at " << tmpl.string()
              << " — this agent may use its own CLI to create cases (see its AGENTS.md).\n";
    return 1;
  }
  if (taken(target)) {
    std::cerr << "Case already exists: " << target.string() << "\n";
    return 1;
  }
  copy_tree(tmpl, target);
  // target.yaml.example -> target.yaml (overwrites a same-named file, like mv).
  for (const auto& p : regular_files(target)) {
    const std::string file = p.filename().string();
    if (file.size() > 8 && file.ends_with(".example")) fs::rename(p, p.parent_path() / file.substr(0, file.size() - 8));
  }
  // Fill {{CASE_NAME}} in text files (no NUL byte), as `grep -I` + sed did.
  for (const auto& p : regular_files(target)) {
    if (read_file(p).find('\0') != std::string::npos) continue;
    substitute(p, {{"{{CASE_NAME}}", name}});
  }
  std::cout << "\nCreated case: " << target.string() << "\n"
            << "Next from your workspace-root session:\n"
            << "  1. Fill in " << target.string() << "/target.yaml.\n"
            << "  2. Read " << agent_dir.string() << "/AGENTS.md for the procedure.\n"
            << "  3. Activate " << target.string() << " with $workspace-coach and run it.\n";
  return 0;
}

}  // namespace

int cmd_new(const std::vector<std::string>& args) {
  if (args.empty() || args[0] == "-h" || args[0] == "--help") {
    (args.empty() ? std::cerr : std::cout) << NEW_USAGE;
    return args.empty() ? 2 : 0;
  }
  const std::vector<std::string> rest(args.begin() + 1, args.end());
  if (rest.size() == 1 && (rest[0] == "-h" || rest[0] == "--help")) {
    std::cout << NEW_USAGE;
    return 0;
  }
  if (args[0] == "project") return new_project(rest);
  if (args[0] == "research") return new_research(rest);
  if (args[0] == "agent-case") return new_agent_case(rest);
  std::cerr << "ws new: unknown kind '" << args[0] << "'\n\n" << NEW_USAGE;
  return 2;
}

}  // namespace ws
