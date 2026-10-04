// `ws firecrawl configure` and `ws firecrawl mcp` — ports of bin/configure-firecrawl and
// bin/firecrawl-mcp-wrapper. The credential file stays a shell file
// (`export FIRECRAWL_API_KEY=<bash printf %q>`), so files written by either version
// work with both.
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "commands.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws {
namespace {

const char* FIRECRAWL_USAGE =
    "Usage: ws firecrawl configure   save the Firecrawl API key (mode 600) and add the MCP server\n"
    "                                to Codex and Claude Code (key from $FIRECRAWL_API_KEY or a prompt)\n"
    "       ws firecrawl mcp         run the Firecrawl MCP server with the saved key (npx firecrawl-mcp)\n";

struct Fail {
  int code;
  std::string msg;
};

// `${XDG_CONFIG_HOME:-$HOME/.config}/ai-workspace`; HOME is needed only as the fallback.
std::string secret_dir() {
  if (const char* x = std::getenv("XDG_CONFIG_HOME"); x && *x) return std::string(x) + "/ai-workspace";
  const char* h = std::getenv("HOME");
  if (!h) throw Fail{1, "ws firecrawl: HOME is not set"};
  return std::string(h) + "/.config/ai-workspace";
}

// bash 3.2 `printf %q` (the system bash on macOS): '' when empty; $'…' when there is a
// control character; otherwise backslashes before shell-special characters.
std::string shell_quote(const std::string& s) {
  if (s.empty()) return "''";
  bool control = false;
  for (unsigned char c : s) control = control || c < 32 || c == 127;
  if (control) {
    std::string out = "$'";
    for (unsigned char c : s) {
      switch (c) {
        case '\a': out += "\\a"; break;
        case '\b': out += "\\b"; break;
        case 27: out += "\\E"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\v': out += "\\v"; break;
        case '\\': out += "\\\\"; break;
        case '\'': out += "\\'"; break;
        default:
          if (c < 32 || c == 127) {
            char b[8];
            std::snprintf(b, sizeof b, "\\%03o", c);
            out += b;
          } else {
            out += static_cast<char>(c);
          }
      }
    }
    return out + "'";
  }
  static const std::string special = " !\"$&'()*,;<>?[\\]^`{|}";
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (special.find(s[i]) != std::string::npos || (i == 0 && s[i] == '#')) out += '\\';
    out += s[i];
  }
  return out;
}

// `read -r -s -p PROMPT key`: prompt and no echo only on a terminal; one byte at a time so
// later children (codex, claude) see the rest of stdin; leading/trailing blanks trimmed.
// nullopt on EOF before a newline (the script's `set -e` then exited 1).
std::optional<std::string> read_secret(const char* prompt) {
  const bool tty = isatty(0);
  termios saved{};
  if (tty) {
    std::cerr << prompt << std::flush;
    tcgetattr(0, &saved);
    termios quiet = saved;
    quiet.c_lflag &= static_cast<tcflag_t>(~ECHO);
    tcsetattr(0, TCSAFLUSH, &quiet);
  }
  std::string line;
  bool newline = false;
  for (;;) {
    char c;
    ssize_t n = ::read(0, &c, 1);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    if (c == '\n') {
      newline = true;
      break;
    }
    if (c != '\0') line += c;
  }
  if (tty) tcsetattr(0, TCSAFLUSH, &saved);
  if (!newline) return std::nullopt;
  const size_t b = line.find_first_not_of(" \t"), e = line.find_last_not_of(" \t");
  return b == std::string::npos ? "" : line.substr(b, e - b + 1);
}

// Run a configured client like the script did (stdio inherited); false if it fails.
bool add_mcp(const std::string& cli, const std::string& wrapper) {
  std::cout.flush();
  try {
    return run_inherit({cli, "mcp", "add", "firecrawl", "--", wrapper}) == 0;
  } catch (const std::exception&) {
    return false;
  }
}

int configure(const std::vector<std::string>& args) {
  for (const auto& a : args)
    if (a == "-h" || a == "--help") return std::cout << FIRECRAWL_USAGE, 0;  // other arguments ignored, as before
  const std::string wrapper = (find_workspace() / "bin" / "firecrawl-mcp-wrapper").string();
  if (!find_on_path("npx")) throw Fail{1, "npx is required for the local Firecrawl MCP server."};
  const bool codex = find_on_path("codex").has_value(), claude = find_on_path("claude").has_value();
  if (!codex && !claude) throw Fail{1, "Install Codex or Claude Code before configuring Firecrawl."};

  std::string key;
  if (const char* k = std::getenv("FIRECRAWL_API_KEY"); k && *k) {
    key = k;
  } else {
    auto line = read_secret("Firecrawl API key: ");
    if (!line) throw Fail{1, ""};
    key = *line;
    std::cout << "\n";
  }
  if (key.empty()) throw Fail{2, "No API key provided."};

  const std::string dir = secret_dir(), file = dir + "/firecrawl.env";
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec || ::chmod(dir.c_str(), 0700) != 0) throw Fail{1, "ws firecrawl: cannot create " + dir};
  // Created and narrowed to 0600 before the key is written (the script wrote first, then chmod).
  const int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (fd < 0 || ::fchmod(fd, 0600) != 0) throw Fail{1, "ws firecrawl: cannot write " + file + ": " + std::strerror(errno)};
  const std::string line = "export FIRECRAWL_API_KEY=" + shell_quote(key) + "\n";
  const bool ok = ::write(fd, line.data(), line.size()) == static_cast<ssize_t>(line.size());
  ::close(fd);
  key.assign(key.size(), '\0');
  if (!ok) throw Fail{1, "ws firecrawl: cannot write " + file};
  std::cout << "Credential saved locally with mode 600: " << file << "\n";

  int configured = 0;
  const std::pair<const char*, const char*> clients[] = {{"codex", "Codex"}, {"claude", "Claude Code"}};
  for (const auto& [cli, label] : clients) {
    if (!(cli == std::string("codex") ? codex : claude)) {
      std::cout << "skip " << label << ": command not installed\n";
      continue;
    }
    std::cout << "Configuring Firecrawl MCP for " << label << "...\n";
    if (add_mcp(cli, wrapper)) {
      ++configured;
    } else {
      std::cout.flush();
      std::cerr << (cli == std::string("codex") ? "Codex" : "Claude") << " configuration failed. It may already have a server named firecrawl.\n";
    }
  }
  if (configured == 0) throw Fail{1, "No clients were configured. The credential file was created but no MCP entry was added."};
  std::cout << "Done. Restart each CLI and use /mcp to verify Firecrawl.\n";
  return 0;
}

// ---------------------------------------------------------------- reading the credential file

// The shell subset the file uses: comments, blank lines, and `[export] NAME=WORD` where
// WORD may mix plain text, \-escapes, '…', "…" (no expansions), and $'…'. Anything else
// (expansions, commands) is rejected instead of executed.
struct Assignment {
  std::string name, value;
  bool exported;
};

void ansi_escape(const std::string& s, size_t& i, std::string& out) {  // after the backslash in $'…'
  const char c = s[i++];
  auto hex = [&](size_t max) {
    unsigned long v = 0;
    size_t n = 0;
    while (n < max && i < s.size() && std::isxdigit(static_cast<unsigned char>(s[i]))) v = v * 16 + std::stoul(std::string(1, s[i++]), nullptr, 16), ++n;
    return n ? std::optional<unsigned long>(v) : std::nullopt;
  };
  switch (c) {
    case 'a': out += '\a'; return;
    case 'b': out += '\b'; return;
    case 'e': case 'E': out += '\x1b'; return;
    case 'f': out += '\f'; return;
    case 'n': out += '\n'; return;
    case 'r': out += '\r'; return;
    case 't': out += '\t'; return;
    case 'v': out += '\v'; return;
    case '\\': case '\'': case '"': case '?': out += c; return;
    case 'x':
      if (auto v = hex(2)) out += static_cast<char>(*v);
      else out += "\\x";
      return;
    default:
      if (c >= '0' && c <= '7') {
        unsigned v = static_cast<unsigned>(c - '0');
        for (int n = 1; n < 3 && i < s.size() && s[i] >= '0' && s[i] <= '7'; ++n) v = v * 8 + static_cast<unsigned>(s[i++] - '0');
        out += static_cast<char>(v & 0xFF);
        return;
      }
      out += '\\';
      out += c;
  }
}

std::vector<Assignment> parse_env_file(const std::string& text, const std::string& file) {
  std::vector<Assignment> out;
  size_t i = 0, line = 1;
  auto bad = [&](const std::string& why) -> Fail {
    return Fail{1, "ws firecrawl: " + file + ":" + std::to_string(line) + ": " + why + "; expected `export NAME=value` lines"};
  };
  auto blank = [](char c) { return c == ' ' || c == '\t'; };
  while (i < text.size()) {
    while (i < text.size() && blank(text[i])) ++i;
    if (i >= text.size()) break;
    if (text[i] == '\n' || text[i] == ';') { line += text[i] == '\n'; ++i; continue; }
    if (text[i] == '#') { while (i < text.size() && text[i] != '\n') ++i; continue; }
    bool exported = false;
    if (text.compare(i, 6, "export") == 0 && i + 6 < text.size() && blank(text[i + 6])) {
      exported = true;
      i += 6;
      while (i < text.size() && blank(text[i])) ++i;
    }
    for (;;) {  // one or more NAME=WORD in this statement
    size_t n = i;
    while (n < text.size() && (std::isalnum(static_cast<unsigned char>(text[n])) || text[n] == '_')) ++n;
    if (n == i || std::isdigit(static_cast<unsigned char>(text[i])) || n >= text.size() || text[n] != '=') throw bad("unsupported line");
    Assignment a{text.substr(i, n - i), "", exported};
    i = n + 1;
    if (i < text.size() && text[i] == '~') throw bad("unsupported '~'");
    while (i < text.size() && !blank(text[i]) && text[i] != '\n' && text[i] != ';') {
      const char c = text[i];
      if (c == '\\') {
        if (i + 1 >= text.size()) { ++i; break; }
        if (text[i + 1] == '\n') ++line;
        else a.value += text[i + 1];
        i += 2;
      } else if (c == '\'') {
        const size_t e = text.find('\'', i + 1);
        if (e == std::string::npos) throw bad("unterminated quote");
        for (size_t k = i + 1; k < e; ++k) line += text[k] == '\n';
        a.value += text.substr(i + 1, e - i - 1);
        i = e + 1;
      } else if (c == '$' && i + 1 < text.size() && text[i + 1] == '\'') {
        i += 2;
        while (i < text.size() && text[i] != '\'') {
          if (text[i] == '\\' && i + 1 < text.size()) ++i, ansi_escape(text, i, a.value);
          else line += text[i] == '\n', a.value += text[i++];
        }
        if (i >= text.size()) throw bad("unterminated quote");
        ++i;
      } else if (c == '"') {
        ++i;
        while (i < text.size() && text[i] != '"') {
          if (text[i] == '$' || text[i] == '`') throw bad("expansions are not supported");
          if (text[i] == '\\' && i + 1 < text.size() && std::strchr("$`\"\\\n", text[i + 1])) {
            if (text[i + 1] != '\n') a.value += text[i + 1];
            else ++line;
            i += 2;
          } else {
            line += text[i] == '\n';
            a.value += text[i++];
          }
        }
        if (i >= text.size()) throw bad("unterminated quote");
        ++i;
      } else if (std::strchr("$`|&<>()", c)) {
        throw bad(std::string("unsupported '") + c + "'");
      } else {
        a.value += text[i++];
      }
    }
    out.push_back(std::move(a));
    while (i < text.size() && blank(text[i])) ++i;
    if (i < text.size() && text[i] == '#') while (i < text.size() && text[i] != '\n') ++i;
    if (i >= text.size() || text[i] == '\n' || text[i] == ';') break;
    }
  }
  return out;
}

int mcp(const std::vector<std::string>& args) {
  for (const auto& a : args)
    if (a == "-h" || a == "--help") return std::cout << FIRECRAWL_USAGE, 0;
  const std::string file = secret_dir() + "/firecrawl.env";
  if (::access(file.c_str(), R_OK) != 0)
    throw Fail{1, "Missing Firecrawl credentials: " + file + "\nRun the workspace command: ./bin/configure-firecrawl"};
  std::string text;
  try {
    text = read_file(file);
  } catch (const std::exception&) {
    throw Fail{1, "ws firecrawl: cannot read " + file};
  }
  // As `source` did: exported names go to the environment; a plain NAME=value only
  // updates a variable that was already exported, otherwise it stays a shell variable.
  std::map<std::string, std::string> shell_only;
  for (const auto& a : parse_env_file(text, file)) {
    if (a.exported || std::getenv(a.name.c_str())) setenv(a.name.c_str(), a.value.c_str(), 1), shell_only.erase(a.name);
    else shell_only[a.name] = a.value;
  }
  const char* env_key = std::getenv("FIRECRAWL_API_KEY");
  const bool has_key = (env_key && *env_key) || (shell_only.count("FIRECRAWL_API_KEY") && !shell_only["FIRECRAWL_API_KEY"].empty());
  if (!has_key) throw Fail{1, "FIRECRAWL_API_KEY: FIRECRAWL_API_KEY is missing from " + file};
  std::cout.flush();
  const char* argv[] = {"npx", "-y", "firecrawl-mcp", nullptr};
  execvp("npx", const_cast<char* const*>(argv));  // replaces ws, like the script's `exec`
  throw Fail{127, std::string("ws firecrawl: npx: ") + std::strerror(errno)};
}

}  // namespace

int cmd_firecrawl(const std::vector<std::string>& args) {
  try {
    if (!args.empty() && args[0] == "configure") return configure({args.begin() + 1, args.end()});
    if (!args.empty() && args[0] == "mcp") return mcp({args.begin() + 1, args.end()});
    if (!args.empty() && (args[0] == "-h" || args[0] == "--help")) return std::cout << FIRECRAWL_USAGE, 0;
    std::cerr << FIRECRAWL_USAGE;
    return 2;
  } catch (const Fail& f) {
    std::cout.flush();
    if (!f.msg.empty()) std::cerr << f.msg << "\n";
    return f.code;
  }
}

}  // namespace ws
