// `ws export-oss` — port of bin/export-oss (Python).
// Export the reusable workspace framework into a clean, publishable repo: copy only the
// paths in _shared/oss/manifest.txt, strip private blocks, generate empty scaffolding,
// scan the result for secrets and personal data, and sync it into DEST only if the scan
// is clean. Never commits or pushes.
//
// The Python semantics this must keep (pathlib, `re`, argparse, rsync) are noted inline.
#include <locale.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wctype.h>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "commands.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws {
namespace {

const char* EXPORT_USAGE_LINE = "usage: ws export-oss [-h] [--check] [--denylist DENYLIST] [dest]\n";
const char* EXPORT_HELP =
    "usage: ws export-oss [-h] [--check] [--denylist DENYLIST] [dest]\n"
    "\n"
    "Export the reusable workspace framework into a clean, publishable repo.\n"
    "\n"
    "Copies only paths listed in _shared/oss/manifest.txt, strips private blocks,\n"
    "generates empty scaffolding, scans the result for secrets and personal data,\n"
    "and syncs it into DEST only if the scan is clean. Never commits or pushes.\n"
    "\n"
    "Usage: ws export-oss [DEST] [--check] [--denylist FILE]\n"
    "  DEST        default: projects/ai-workspace\n"
    "  --check     build and scan only; do not touch DEST\n"
    "  --denylist  extra words to block (default: <workspace>/.oss-denylist,\n"
    "              one word or phrase per line, # comments allowed)\n";

// An error that ends the command: message on stderr, then exit `code` (sys.exit(msg) = 1).
struct Exit {
  int code;
  std::string msg;
};
[[noreturn]] void die(const std::string& msg, int code = 1) { throw Exit{code, msg}; }

// ---------------------------------------------------------------- paths (pathlib)

using Parts = std::vector<std::string>;

// PurePosixPath parts of a relative path: empty and "." components dropped, ".." kept.
Parts split_parts(const std::string& s) {
  Parts out;
  size_t i = 0;
  while (i <= s.size()) {
    size_t j = s.find('/', i);
    if (j == std::string::npos) j = s.size();
    std::string p = s.substr(i, j - i);
    if (!p.empty() && p != ".") out.push_back(p);
    i = j + 1;
  }
  return out;
}

std::string join_parts(const Parts& p) {
  std::string s;
  for (const auto& x : p) s += (s.empty() ? "" : "/") + x;
  return s;
}

// Path.suffix (Python 3.14): the last ".xxx" of the name after leading dots; "foo." -> ".".
std::string suffix_of(const std::string& name) {
  size_t lead = 0;
  while (lead < name.size() && name[lead] == '.') ++lead;
  const std::string rest = name.substr(lead);
  const size_t i = rest.rfind('.');
  return i == std::string::npos ? "" : rest.substr(i);
}

std::optional<struct stat> lstat_of(const std::string& p) {
  struct stat st{};
  if (::lstat(p.c_str(), &st) != 0) return std::nullopt;
  return st;
}
std::optional<struct stat> stat_of(const std::string& p) {
  struct stat st{};
  if (::stat(p.c_str(), &st) != 0) return std::nullopt;
  return st;
}
bool is_link(const std::string& p) {
  auto st = lstat_of(p);
  return st && S_ISLNK(st->st_mode);
}
bool is_file_follow(const std::string& p) {  // Path.is_file()
  auto st = stat_of(p);
  return st && S_ISREG(st->st_mode);
}
bool is_dir_nofollow(const std::string& p) {
  auto st = lstat_of(p);
  return st && S_ISDIR(st->st_mode);
}

std::string read_link(const std::string& p) {
  std::error_code ec;
  auto t = fs::read_symlink(p, ec);
  if (ec) die("error: readlink " + p + ": " + ec.message());
  return t.string();
}

// Path.rglob("*"): every entry below `root` (relative parts), not descending into
// symlinked folders, sorted like Path objects (by parts).
std::vector<Parts> rglob(const std::string& root) {
  std::vector<Parts> out;
  std::vector<Parts> stack{{}};
  while (!stack.empty()) {
    Parts dir = stack.back();
    stack.pop_back();
    const std::string abs = dir.empty() ? root : root + "/" + join_parts(dir);
    std::error_code ec;
    fs::directory_iterator it(abs, ec);
    if (ec) continue;
    for (const auto& e : it) {
      Parts p = dir;
      p.push_back(e.path().filename().string());
      if (is_dir_nofollow(root + "/" + join_parts(p))) stack.push_back(p);
      out.push_back(std::move(p));
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

// os.path.realpath(path, strict=False) as in Python 3.14 posixpath.
std::string py_realpath(const std::string& filename) {
  std::vector<std::optional<std::string>> rest;  // stack; nullopt marks a resolved symlink
  {
    std::vector<std::string> parts;
    size_t i = 0;
    for (;;) {
      size_t j = filename.find('/', i);
      parts.push_back(filename.substr(i, j == std::string::npos ? std::string::npos : j - i));
      if (j == std::string::npos) break;
      i = j + 1;
    }
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) rest.emplace_back(*it);
  }
  size_t part_count = rest.size();
  std::string path = !filename.empty() && filename[0] == '/' ? "/" : fs::current_path().string();
  std::map<std::string, std::optional<std::string>> seen;
  while (part_count) {
    std::optional<std::string> name = rest.back();
    rest.pop_back();
    if (!name) {
      seen[*rest.back()] = path;
      rest.pop_back();
      continue;
    }
    --part_count;
    if (name->empty() || *name == ".") continue;
    if (*name == "..") {
      path = path.substr(0, path.rfind('/'));
      if (path.empty()) path = "/";
      continue;
    }
    const std::string newpath = path == "/" ? path + *name : path + "/" + *name;
    auto st = lstat_of(newpath);
    if (!st || !S_ISLNK(st->st_mode)) {
      path = newpath;
      continue;
    }
    if (auto s = seen.find(newpath); s != seen.end()) {
      path = s->second ? *s->second : newpath;  // cached, or a loop: give up on this link
      continue;
    }
    std::error_code ec;
    const std::string target = fs::read_symlink(newpath, ec).string();
    if (ec) {
      path = newpath;
      continue;
    }
    if (!target.empty() && target[0] == '/') path = "/";
    seen[newpath] = std::nullopt;
    rest.emplace_back(newpath);
    rest.emplace_back(std::nullopt);
    std::vector<std::string> tparts;
    size_t i = 0;
    for (;;) {
      size_t j = target.find('/', i);
      tparts.push_back(target.substr(i, j == std::string::npos ? std::string::npos : j - i));
      if (j == std::string::npos) break;
      i = j + 1;
    }
    for (auto it = tparts.rbegin(); it != tparts.rend(); ++it) rest.emplace_back(*it);
    part_count += tparts.size();
  }
  return path;
}

// PurePath.is_relative_to, by parts.
bool relative_to(const std::string& child, const std::string& parent) {
  const Parts c = split_parts(child), p = split_parts(parent);
  return c.size() >= p.size() && std::equal(p.begin(), p.end(), c.begin());
}

// ---------------------------------------------------------------- file copies

void copy_times(const std::string& from, const std::string& to) {
  std::error_code ec;
  auto t = fs::last_write_time(from, ec);
  if (!ec) fs::last_write_time(to, t, ec);
}

void chmod_like(const std::string& from, const std::string& to) {  // shutil.copymode
  auto st = stat_of(from);
  if (!st || ::chmod(to.c_str(), st->st_mode & 07777) != 0) die("error: chmod " + to + ": " + std::strerror(errno));
}

void write_bytes(const std::string& p, const std::string& data) {
  FILE* f = std::fopen(p.c_str(), "wb");
  if (!f) die("error: cannot write " + p + ": " + std::strerror(errno));
  const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
  if (std::fclose(f) != 0 || !ok) die("error: cannot write " + p);
}

std::string read_bytes(const std::string& p) {
  try {
    return read_file(p);
  } catch (const std::exception&) {
    die("error: cannot read " + p + ": " + std::strerror(errno));
  }
}

void copy2(const std::string& from, const std::string& to) {  // shutil.copy2: data, mode, mtime
  write_bytes(to, read_bytes(from));
  chmod_like(from, to);
  copy_times(from, to);
}

void make_symlink(const std::string& target, const std::string& at) {
  if (::symlink(target.c_str(), at.c_str()) != 0) die("error: symlink " + at + ": " + std::strerror(errno));
}

void mkdirs(const std::string& p) {
  std::error_code ec;
  fs::create_directories(p, ec);
  if (ec) die("error: mkdir " + p + ": " + ec.message());
}

// ---------------------------------------------------------------- text (Python str semantics)

// Strict UTF-8 decode as Python's codec does; nullopt on any invalid sequence.
std::optional<std::u32string> decode_utf8(const std::string& s) {
  std::u32string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
      out += c;
      ++i;
      continue;
    }
    int n;
    char32_t cp, min;
    if (c >= 0xC2 && c <= 0xDF) n = 1, cp = c & 0x1F, min = 0x80;
    else if (c >= 0xE0 && c <= 0xEF) n = 2, cp = c & 0x0F, min = 0x800;
    else if (c >= 0xF0 && c <= 0xF4) n = 3, cp = c & 0x07, min = 0x10000;
    else return std::nullopt;
    for (int k = 1; k <= n; ++k) {
      if (i + k >= s.size()) return std::nullopt;
      const unsigned char d = static_cast<unsigned char>(s[i + k]);
      if ((d & 0xC0) != 0x80) return std::nullopt;
      cp = (cp << 6) | (d & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return std::nullopt;
    out += cp;
    i += static_cast<size_t>(n) + 1;
  }
  return out;
}

// Text-mode read with newline=None: "\r\n" and lone "\r" become "\n".
std::string universal_newlines(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\r') {
      out += '\n';
      if (i + 1 < s.size() && s[i + 1] == '\n') ++i;
    } else {
      out += s[i];
    }
  }
  return out;
}

size_t count_of(const std::string& s, const std::string& sub) {
  size_t n = 0;
  for (size_t p = s.find(sub); p != std::string::npos; p = s.find(sub, p + sub.size())) ++n;
  return n;
}

// re.sub for `[ \t]*BEGIN.*?END[ \t]*\n?` (DOTALL), where the pattern's END part is
// `[ \t]*END` for the hash form; both reduce to "first END after BEGIN".
std::string remove_blocks(const std::string& text, const std::string& begin, const std::string& end) {
  std::string out;
  size_t pos = 0;
  for (;;) {
    size_t b = text.find(begin, pos);
    if (b == std::string::npos) break;
    size_t e = text.find(end, b + begin.size());
    if (e == std::string::npos) break;  // no END after this BEGIN, so none after a later one
    size_t start = b;
    while (start > pos && (text[start - 1] == ' ' || text[start - 1] == '\t')) --start;
    size_t stop = e + end.size();
    while (stop < text.size() && (text[stop] == ' ' || text[stop] == '\t')) ++stop;
    if (stop < text.size() && text[stop] == '\n') ++stop;
    out.append(text, pos, start - pos);
    pos = stop;
  }
  out.append(text, pos, std::string::npos);
  return out;
}

std::string strip_private(const std::string& rel, std::string text) {
  struct Form {
    const char* marker;
    const char* begin;
    const char* end;
  };
  static const Form forms[] = {
      {"<!-- private:", "<!-- private:begin -->", "<!-- private:end -->"},
      {"# private:", "# private:begin\n", "# private:end"},
  };
  for (const auto& f : forms) {
    if (text.find(f.marker) == std::string::npos) continue;
    const std::string m = f.marker;
    if (count_of(text, m + "begin") != count_of(text, m + "end")) die("error: unbalanced private markers in " + rel);
    text = remove_blocks(text, f.begin, f.end);
  }
  return text;
}

// ---------------------------------------------------------------- scan rules (`re` semantics)

using U = std::u32string;

locale_t utf8_locale() {
  static locale_t loc = [] {
    for (const char* name : {"C.UTF-8", "en_US.UTF-8", "UTF-8", "C.utf8"}) {
      if (locale_t l = newlocale(LC_CTYPE_MASK, name, nullptr)) return l;
    }
    return static_cast<locale_t>(nullptr);
  }();
  return loc;
}

bool ascii_alnum(char32_t c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool ascii_upper(char32_t c) { return c >= 'A' && c <= 'Z'; }

// `\w` for str patterns: Unicode letters and digits, plus "_".
bool is_word(char32_t c) {
  if (c < 0x80) return ascii_alnum(c) || c == '_';
  locale_t l = utf8_locale();
  return l && iswalnum_l(static_cast<wint_t>(c), l);
}

bool boundary(const U& s, size_t i) {  // `\b` at position i
  const bool before = i > 0 && is_word(s[i - 1]);
  const bool after = i < s.size() && is_word(s[i]);
  return before != after;
}

bool at(const U& s, size_t i, const U& lit) { return i + lit.size() <= s.size() && s.compare(i, lit.size(), lit) == 0; }

template <class Pred>
size_t run_len(const U& s, size_t i, Pred in_class) {
  size_t j = i;
  while (j < s.size() && in_class(s[j])) ++j;
  return j - i;
}

// Simple case folding as `re.IGNORECASE` compares characters.
char32_t fold(char32_t c) {
  if (c < 0x80) return (c >= 'A' && c <= 'Z') ? c + 32 : c;
  if (locale_t l = utf8_locale()) c = static_cast<char32_t>(towlower_l(static_cast<wint_t>(c), l));
  static const std::map<char32_t, char32_t> equiv = {
      {0x131, 'i'},     {0x17F, 's'},     {0xB5, 0x3BC},    {0x345, 0x3B9},   {0x1FBE, 0x3B9},  {0x1FD3, 0x390},
      {0x1FE3, 0x3B0},  {0x3D0, 0x3B2},   {0x3F5, 0x3B5},   {0x3D1, 0x3B8},   {0x3F0, 0x3BA},   {0x3D6, 0x3C0},
      {0x3F1, 0x3C1},   {0x3C2, 0x3C3},   {0x3D5, 0x3C6},   {0x1E9B, 0x1E61}, {0xFB06, 0xFB05},
  };
  auto it = equiv.find(c);
  return it == equiv.end() ? c : it->second;
}

// `[A-Za-z0-9]` under IGNORECASE also matches these four.
bool deny_neighbour(char32_t c) { return ascii_alnum(c) || c == 0x130 || c == 0x131 || c == 0x17F || c == 0x212A; }

U to_u(const std::string& ascii_or_utf8) {
  auto d = decode_utf8(ascii_or_utf8);
  return d ? *d : U(ascii_or_utf8.begin(), ascii_or_utf8.end());
}

struct Rule {
  std::string name;
  std::function<bool(const U&)> match;
};

std::vector<Rule> secret_rules() {
  auto key_char = [](char32_t c) { return ascii_alnum(c) || c == '_' || c == '-'; };
  std::vector<Rule> r;
  r.push_back({"anthropic/openai key", [=](const U& s) {  // sk-(?:ant-)?[A-Za-z0-9_-]{20,}
                 for (size_t i = 0; i + 3 <= s.size(); ++i)
                   if (at(s, i, U"sk-") && run_len(s, i + 3, key_char) >= 20) return true;
                 return false;
               }});
  r.push_back({"firecrawl key", [](const U& s) {  // \bfc-[A-Za-z0-9]{20,}
                 for (size_t i = 0; i + 3 <= s.size(); ++i)
                   if (at(s, i, U"fc-") && boundary(s, i) && run_len(s, i + 3, ascii_alnum) >= 20) return true;
                 return false;
               }});
  r.push_back({"aws access key", [](const U& s) {  // \bAKIA[0-9A-Z]{16}\b
                 auto cls = [](char32_t c) { return (c >= '0' && c <= '9') || ascii_upper(c); };
                 for (size_t i = 0; i + 20 <= s.size(); ++i)
                   if (at(s, i, U"AKIA") && boundary(s, i) && run_len(s, i + 4, cls) >= 16 && boundary(s, i + 20)) return true;
                 return false;
               }});
  r.push_back({"github token", [](const U& s) {  // \bgh[pousr]_[A-Za-z0-9]{30,}
                 for (size_t i = 0; i + 4 <= s.size(); ++i)
                   if (at(s, i, U"gh") && boundary(s, i) && U(U"pousr").find(s[i + 2]) != U::npos && s[i + 3] == '_' &&
                       run_len(s, i + 4, ascii_alnum) >= 30)
                     return true;
                 return false;
               }});
  r.push_back({"slack token", [](const U& s) {  // \bxox[abprs]-[A-Za-z0-9-]{10,}
                 auto cls = [](char32_t c) { return ascii_alnum(c) || c == '-'; };
                 for (size_t i = 0; i + 5 <= s.size(); ++i)
                   if (at(s, i, U"xox") && boundary(s, i) && U(U"abprs").find(s[i + 3]) != U::npos && s[i + 4] == '-' &&
                       run_len(s, i + 5, cls) >= 10)
                     return true;
                 return false;
               }});
  r.push_back({"private key", [](const U& s) {  // -----BEGIN [A-Z ]*PRIVATE KEY-----
                 const U head = U"-----BEGIN ", tail = U"PRIVATE KEY-----";
                 for (size_t i = s.find(head); i != U::npos; i = s.find(head, i + 1)) {
                   for (size_t j = i + head.size();; ++j) {
                     if (at(s, j, tail)) return true;
                     if (j >= s.size() || !(ascii_upper(s[j]) || s[j] == ' ')) break;
                   }
                 }
                 return false;
               }});
  r.push_back({"aws hostname", [](const U& s) {  // \b[a-z0-9.-]+\.compute(?:-1)?\.amazonaws\.com\b
                 auto cls = [](char32_t c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-'; };
                 for (const U& lit : {U(U".compute.amazonaws.com"), U(U".compute-1.amazonaws.com")}) {
                   for (size_t k = s.find(lit); k != U::npos; k = s.find(lit, k + 1)) {
                     if (!boundary(s, k + lit.size())) continue;
                     size_t p0 = k;
                     while (p0 > 0 && cls(s[p0 - 1])) --p0;
                     for (size_t p = p0; p < k; ++p)
                       if (boundary(s, p)) return true;
                   }
                 }
                 return false;
               }});
  return r;
}

Rule home_rule(const U& home) {  // re.escape(home) + r"\b"
  return {"home path", [home](const U& s) {
            if (home.empty()) {  // a bare `\b`
              for (size_t i = 0; i <= s.size(); ++i)
                if (boundary(s, i)) return true;
              return false;
            }
            for (size_t i = s.find(home); i != U::npos; i = s.find(home, i + 1))
              if (boundary(s, i + home.size())) return true;
            return false;
          }};
}

Rule deny_rule(const std::string& word) {  // (?<![A-Za-z0-9])WORD(?![A-Za-z0-9]), IGNORECASE
  U w = to_u(word);
  for (auto& c : w) c = fold(c);
  return {"denylist '" + word + "'", [w](const U& s) {
            if (w.size() > s.size()) return false;
            for (size_t i = 0; i + w.size() <= s.size(); ++i) {
              if (i > 0 && deny_neighbour(s[i - 1])) continue;
              size_t k = 0;
              while (k < w.size() && fold(s[i + k]) == w[k]) ++k;
              if (k < w.size()) continue;
              if (i + w.size() < s.size() && deny_neighbour(s[i + w.size()])) continue;
              return true;
            }
            return false;
          }};
}

// EMAIL.finditer + EMAIL_OK: count the matches whose domain is not allowed.
size_t bad_emails(const U& s) {
  auto local = [](char32_t c) { return ascii_alnum(c) || c == '.' || c == '_' || c == '%' || c == '+' || c == '-'; };
  auto dom = [](char32_t c) { return ascii_alnum(c) || c == '.' || c == '-'; };
  auto alpha = [](char32_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
  static const std::set<std::string> ok = {"example.com", "example.org", "anthropic.com", "users.noreply.github.com"};
  size_t n = 0, pos = 0;
  for (size_t a = s.find(U'@', pos); a != U::npos; a = s.find(U'@', std::max(pos, a + 1))) {
    size_t st = a;
    while (st > pos && local(s[st - 1])) --st;
    if (st == a) continue;  // no local part
    const size_t e = a + 1 + run_len(s, a + 1, dom);
    // Greedy `[A-Za-z0-9.-]+` then `\.[A-Za-z]{2,}`: the last '.' that leaves >= 2 letters.
    size_t end = 0;
    for (size_t j = e; j-- > a + 2;) {
      if (s[j] == '.' && run_len(s, j + 1, alpha) >= 2) {
        end = j + 1 + run_len(s, j + 1, alpha);
        break;
      }
    }
    if (!end) continue;
    std::string domain;
    for (size_t i = a + 1; i < end; ++i) domain += static_cast<char>(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
    if (!ok.count(domain)) ++n;
    pos = end;
  }
  return n;
}

// str.splitlines() on already newline-normalised text.
std::vector<U> split_lines(const U& text) {
  static const U seps = U"\n\v\f\x1c\x1d\x1e\x85\u2028\u2029";
  std::vector<U> lines;
  size_t i = 0;
  while (i < text.size()) {
    size_t j = i;
    while (j < text.size() && seps.find(text[j]) == U::npos) ++j;
    lines.push_back(text.substr(i, j - i));
    i = j + 1;
  }
  return lines;
}

std::vector<std::string> scan(const std::string& stage, const std::vector<std::string>& deny) {
  std::string home;
  if (const char* h = std::getenv("HOME")) home = h;
  std::vector<Rule> rules = secret_rules();
  rules.push_back(home_rule(to_u(home)));
  std::set<std::string> names;  // a dict: a repeated word keeps one rule
  for (const auto& w : deny)
    if (names.insert(w).second) rules.push_back(deny_rule(w));

  const std::string stage_real = py_realpath(stage);
  std::vector<std::string> hits;
  for (const Parts& p : rglob(stage)) {
    const std::string rel = join_parts(p);
    const std::string abs = stage + "/" + rel;
    if (is_link(abs)) {
      const std::string target = read_link(abs);
      Parts parent = p;
      parent.pop_back();
      const std::string base = parent.empty() ? stage : stage + "/" + join_parts(parent);
      const std::string joined = !target.empty() && target[0] == '/' ? target : base + "/" + target;
      const bool inside = relative_to(py_realpath(joined), stage_real);
      if ((!target.empty() && target[0] == '/') || !inside) hits.push_back(rel + ": symlink leaves the repo -> " + target);
      continue;
    }
    if (!is_file_follow(abs)) continue;
    auto text = decode_utf8(universal_newlines(read_bytes(abs)));
    if (!text) continue;
    const std::vector<U> lines = split_lines(*text);
    for (size_t i = 0; i < lines.size(); ++i) {
      const std::string loc = rel + ":" + std::to_string(i + 1) + ": ";
      for (const auto& r : rules)
        if (r.match(lines[i])) hits.push_back(loc + r.name);
      for (size_t k = bad_emails(lines[i]); k > 0; --k) hits.push_back(loc + "email address");
    }
  }
  return hits;
}

// ---------------------------------------------------------------- staging

const std::set<std::string> SKIP_NAMES = {".DS_Store", "__pycache__", ".pytest_cache", ".ruff_cache", ".venv", "node_modules", ".git"};
const std::vector<std::string> SKIP_SUFFIXES = {".pyc", ".bak", ".orig"};
const std::vector<std::string> SKIP_PATHS = {
    "_shared/skills/workspace-coach/references/verification-agent-workflow.md",
    "tools/local-research/work",
    "tools/local-research/tests/output",
    "tools/ws/build",
};
const std::set<std::string> TEXT_SUFFIXES = {".md", ".sh", ".py", ".yml", ".yaml", ".json", ".toml", ".txt", ".js", ".ts", ""};

bool ends_with(const std::string& s, const std::string& t) { return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0; }
bool starts_with(const std::string& s, const std::string& t) { return s.compare(0, t.size(), t) == 0; }

bool skipped(const std::string& rel) {
  const Parts parts = split_parts(rel);
  for (const auto& p : parts)
    if (SKIP_NAMES.count(p)) return true;
  for (const auto& s : SKIP_SUFFIXES)
    if (ends_with(rel, s)) return true;
  for (const auto& s : SKIP_PATHS)
    if (rel == s || starts_with(rel, s + "/")) return true;
  if (parts.empty()) return false;
  if (parts[0] == ".env" || (starts_with(parts.back(), ".env") && parts.back() != ".env.example")) return true;
  // agents/<agent>/cases/<case>: only the _template case is framework
  return parts.size() >= 4 && parts[0] == "agents" && parts[2] == "cases" && parts[3] != "_template";
}

size_t copy_entry(const std::string& ws, const std::string& rel_in, const std::string& stage) {
  const Parts rel_parts = split_parts(rel_in);
  const std::string src = rel_parts.empty() ? ws : ws + "/" + join_parts(rel_parts);
  if (!lstat_of(src)) die("error: manifest path missing: " + rel_in);
  std::vector<Parts> files;
  if (is_file_follow(src) || is_link(src)) {
    files.push_back(rel_parts);
  } else {
    for (Parts p : rglob(src)) {
      const std::string abs = src + "/" + join_parts(p);
      if (!(is_file_follow(abs) || is_link(abs))) continue;
      Parts full = rel_parts;
      full.insert(full.end(), p.begin(), p.end());
      files.push_back(std::move(full));
    }
  }
  size_t n = 0;
  for (const Parts& fp : files) {
    const std::string r = join_parts(fp);
    if (skipped(r)) continue;
    const std::string f = ws + "/" + r;
    const std::string dst = stage + "/" + r;
    mkdirs(fs::path(dst).parent_path().string());
    if (is_link(f)) {
      make_symlink(read_link(f), dst);
    } else if (starts_with(r, "_shared/oss/")) {
      // The export's own templates are copied verbatim, so a clone can export too
      // (its README quotes the private markers; stripping would cut it).
      copy2(f, dst);
    } else if (TEXT_SUFFIXES.count(suffix_of(fp.back()))) {
      const std::string text = universal_newlines(read_bytes(f));
      if (!decode_utf8(text)) {
        copy2(f, dst);
      } else {
        write_bytes(dst, strip_private(r, text));
        chmod_like(f, dst);
      }
    } else {
      copy2(f, dst);
    }
    ++n;
  }
  return n;
}

void touch(const std::string& p) {
  if (lstat_of(p)) {
    std::error_code ec;
    fs::last_write_time(p, fs::file_time_type::clock::now(), ec);
  } else {
    write_bytes(p, "");
  }
}

void generate(const std::string& ws, const std::string& stage) {
  const std::string oss = ws + "/_shared/oss";
  const std::pair<const char*, const char*> files[] = {
      {"README.md", "README.md"},
      {".gitignore", "gitignore"},
      {"LICENSE", "LICENSE"},
      {"knowledge/README.md", "knowledge-README.md"},
      {"projects/README.md", "projects-README.md"},
      {"temp-sessions/README.md", "temp-sessions-README.md"},
  };
  for (const auto& [rel, src] : files) {
    mkdirs(fs::path(stage + "/" + rel).parent_path().string());
    if (!stat_of(oss + "/" + src)) die(std::string("error: missing ") + oss + "/" + src);
    copy2(oss + "/" + src, stage + "/" + rel);
  }
  for (const char* keep : {"research/topics", "worktrees", "knowledge/inbox", "knowledge/daily", "knowledge/indexes"}) {
    mkdirs(stage + "/" + keep);
    touch(stage + "/" + keep + "/.gitkeep");
  }
  mkdirs(stage + "/tools");
  write_bytes(stage + "/tools/database-port-registry.yaml",
              "schema_version: '1.0'\n"
              "scope: local_database_endpoints\n"
              "policy:\n"
              "  bind_host: 127.0.0.1\n"
              "  notes:\n"
              "  - Entries record ownership and purpose, not whether a process is currently live.\n"
              "  - Do not reuse a port unless owner, case, role, and destination all match.\n"
              "  - Store no credentials or connection strings here.\n"
              "assignments: []\n");
  const std::string skills = stage + "/.kiro/skills";
  mkdirs(skills);
  const std::string shared = stage + "/_shared/skills";
  std::vector<std::string> names;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(shared, ec)) names.push_back(e.path().filename().string());
  if (ec) die("error: " + shared + ": " + ec.message());
  std::sort(names.begin(), names.end());
  for (const auto& n : names)
    if (is_file_follow(shared + "/" + n + "/SKILL.md")) make_symlink("../../_shared/skills/" + n, skills + "/" + n);
}

// ---------------------------------------------------------------- sync (rsync -a --delete --exclude .git)

// Remove `p` like rsync --delete: everything except entries named .git; folders that
// still hold one are kept.
void delete_tree(const std::string& p) {
  if (is_dir_nofollow(p)) {
    std::error_code ec;
    std::vector<std::string> kids;
    for (const auto& e : fs::directory_iterator(p, ec)) kids.push_back(e.path().filename().string());
    for (const auto& k : kids)
      if (k != ".git") delete_tree(p + "/" + k);
    if (::rmdir(p.c_str()) != 0 && (errno == ENOTEMPTY || errno == EEXIST))  // a .git is inside: keep it
      std::cerr << "ws export-oss: warning: " << p << ": not empty, cannot delete\n";
  } else {
    ::unlink(p.c_str());
  }
}

void sync_dir(const std::string& s, const std::string& d) {
  std::set<std::string> names;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(s, ec)) {
    const std::string n = e.path().filename().string();
    if (n == ".git") continue;
    names.insert(n);
    const std::string sp = s + "/" + n, dp = d + "/" + n;
    const auto sst = lstat_of(sp);
    const auto dst = lstat_of(dp);
    if (!sst) continue;
    if (S_ISLNK(sst->st_mode)) {
      const std::string target = read_link(sp);
      if (dst && S_ISLNK(dst->st_mode) && read_link(dp) == target) continue;
      if (dst) delete_tree(dp);
      make_symlink(target, dp);
    } else if (S_ISDIR(sst->st_mode)) {
      if (dst && !S_ISDIR(dst->st_mode)) delete_tree(dp);
      if (!dst || !S_ISDIR(dst->st_mode)) mkdirs(dp);
      sync_dir(sp, dp);
      chmod_like(sp, dp);
      copy_times(sp, dp);
    } else if (S_ISREG(sst->st_mode)) {
      const bool same = dst && S_ISREG(dst->st_mode) && dst->st_size == sst->st_size &&
                        fs::last_write_time(sp, ec) == fs::last_write_time(dp, ec);
      if (!same) {
        if (dst && !S_ISREG(dst->st_mode)) delete_tree(dp);
        const std::string tmp = d + "/." + n + "." + random_hex(6);
        copy2(sp, tmp);
        if (::rename(tmp.c_str(), dp.c_str()) != 0) die("error: rename " + dp + ": " + std::strerror(errno));
      } else {
        chmod_like(sp, dp);
      }
    }
  }
  std::vector<std::string> extra;
  for (const auto& e : fs::directory_iterator(d, ec)) {
    const std::string n = e.path().filename().string();
    if (n != ".git" && !names.count(n)) extra.push_back(n);
  }
  for (const auto& n : extra) delete_tree(d + "/" + n);
}

// ---------------------------------------------------------------- arguments (argparse)

struct Args {
  std::optional<std::string> dest;
  bool check = false;
  std::optional<std::string> denylist;
};

[[noreturn]] void usage_error(const std::string& msg) { die(std::string(EXPORT_USAGE_LINE) + "ws export-oss: error: " + msg, 2); }

// Returns nullopt after printing help.
std::optional<Args> parse(const std::vector<std::string>& argv) {
  Args a;
  std::vector<std::string> extras;
  bool only_positional = false;
  for (size_t i = 0; i < argv.size(); ++i) {
    const std::string& t = argv[i];
    if (only_positional || t == "-" || t.empty() || t[0] != '-') {
      if (!a.dest) a.dest = t;
      else extras.push_back(t);
      continue;
    }
    if (t == "--") {
      only_positional = true;
      continue;
    }
    if (t == "-h") {
      std::cout << EXPORT_HELP;
      return std::nullopt;
    }
    if (t.size() > 2 && t[1] == '-') {
      const size_t eq = t.find('=');
      const std::string name = t.substr(0, eq);
      std::string full;
      for (const char* opt : {"--help", "--check", "--denylist"})  // unique prefixes are accepted
        if (starts_with(opt, name)) full = opt;
      if (full == "--help") {
        std::cout << EXPORT_HELP;
        return std::nullopt;
      }
      if (full == "--check") {
        if (eq != std::string::npos) usage_error("argument --check: ignored explicit argument '" + t.substr(eq + 1) + "'");
        a.check = true;
        continue;
      }
      if (full == "--denylist") {
        if (eq != std::string::npos) {
          a.denylist = t.substr(eq + 1);
        } else if (i + 1 < argv.size() && (argv[i + 1].empty() || argv[i + 1][0] != '-' || argv[i + 1] == "-")) {
          a.denylist = argv[++i];
        } else {
          usage_error("argument --denylist: expected one argument");
        }
        continue;
      }
    }
    extras.push_back(t);
  }
  if (!extras.empty()) {
    std::string s;
    for (const auto& e : extras) s += (s.empty() ? "" : " ") + e;
    usage_error("unrecognized arguments: " + s);
  }
  return a;
}

std::vector<std::string> nonblank_lines(const std::string& text, bool lstrip_comment) {
  std::vector<std::string> out;
  const auto d = decode_utf8(universal_newlines(text));
  if (!d) die("error: not UTF-8 text");
  for (const U& line : split_lines(*d)) {
    std::string l;
    for (char32_t c : line) {  // re-encode
      if (c < 0x80) l += static_cast<char>(c);
      else if (c < 0x800) l += static_cast<char>(0xC0 | (c >> 6)), l += static_cast<char>(0x80 | (c & 0x3F));
      else if (c < 0x10000)
        l += static_cast<char>(0xE0 | (c >> 12)), l += static_cast<char>(0x80 | ((c >> 6) & 0x3F)), l += static_cast<char>(0x80 | (c & 0x3F));
      else
        l += static_cast<char>(0xF0 | (c >> 18)), l += static_cast<char>(0x80 | ((c >> 12) & 0x3F)),
            l += static_cast<char>(0x80 | ((c >> 6) & 0x3F)), l += static_cast<char>(0x80 | (c & 0x3F));
    }
    const std::string s = trim(l);
    if (s.empty()) continue;
    // denylist: `l.lstrip().startswith("#")`; manifest: `l.startswith("#")` on the raw line
    if (lstrip_comment ? s[0] == '#' : (!l.empty() && l[0] == '#')) continue;
    out.push_back(s);
  }
  return out;
}

int run_export(const Args& a) {
  const std::string ws = find_workspace().string();
  const std::string dest_arg = a.dest.value_or(ws + "/projects/ai-workspace");
  const std::string deny_path = a.denylist.value_or(ws + "/.oss-denylist");

  std::vector<std::string> deny;
  if (is_file_follow(deny_path)) deny = nonblank_lines(read_bytes(deny_path), true);
  const std::vector<std::string> manifest = nonblank_lines(read_bytes(ws + "/_shared/oss/manifest.txt"), false);

  std::string tmpdir = "/tmp";
  if (const char* t = std::getenv("TMPDIR"); t && *t) tmpdir = t;
  std::string tmpl = tmpdir + "/tmpXXXXXXXX";
  if (!mkdtemp(tmpl.data())) die("error: cannot create a temp folder in " + tmpdir);
  struct Cleanup {
    std::string p;
    ~Cleanup() {
      std::error_code ec;
      fs::remove_all(p, ec);
    }
  } cleanup{tmpl};
  const std::string stage = tmpl + "/stage";
  mkdirs(stage);
  ::chmod(stage.c_str(), 0777 & ~[] {
    mode_t m = ::umask(0);
    ::umask(m);
    return m;
  }());

  size_t count = 0;
  for (const auto& rel : manifest) count += copy_entry(ws, rel, stage);
  generate(ws, stage);
  const std::vector<std::string> hits = scan(stage, deny);
  unsigned long long size = 0;
  for (const Parts& p : rglob(stage)) {
    auto st = lstat_of(stage + "/" + join_parts(p));
    if (st && S_ISREG(st->st_mode)) size += static_cast<unsigned long long>(st->st_size);
  }
  char kib[64];
  std::snprintf(kib, sizeof kib, "%.0f", static_cast<double>(size) / 1024.0);
  std::cout << "staged " << count << " framework files (+ generated scaffolding), " << kib << " KiB; denylist terms: " << deny.size()
            << "\n";
  if (!hits.empty()) {
    std::cout << "BLOCKED: " << hits.size() << " finding(s); nothing written:\n";
    for (size_t i = 0; i < hits.size() && i < 50; ++i) std::cout << "  " << hits[i] << "\n";
    return 1;
  }
  std::cout << "scan clean\n";
  if (a.check) return 0;

  const std::string dest = py_realpath(dest_arg);
  mkdirs(dest);
  std::cout.flush();
  sync_dir(stage, dest);
  chmod_like(stage, dest);
  copy_times(stage, dest);
  if (!stat_of(dest + "/.git")) {
    auto r = run({"git", "-C", dest, "init", "-q"}, RunOptions{});
    std::cerr << r.err;
    if (r.code != 0) die("error: git init failed in " + dest);
  }
  auto st = run({"git", "-C", dest, "status", "--short"}, RunOptions{});
  std::cerr << st.err;
  size_t changed = 0;
  for (char c : st.out) changed += c == '\n';
  if (!st.out.empty() && st.out.back() != '\n') ++changed;
  std::cout << "exported to " << dest << "; " << changed << " path(s) changed vs last commit (nothing committed)\n";
  return 0;
}

}  // namespace

int cmd_export_oss(const std::vector<std::string>& args) {
  try {
    auto a = parse(args);
    if (!a) return 0;
    return run_export(*a);
  } catch (const Exit& e) {
    std::cout.flush();
    std::cerr << e.msg << "\n";
    return e.code;
  }
}

}  // namespace ws
