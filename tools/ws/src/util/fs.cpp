#include "util/fs.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <stdexcept>

namespace ws {

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + p.string());
  return std::string(std::istreambuf_iterator<char>(in), {});
}

void write_file(const fs::path& p, const std::string& content) {
  if (p.has_parent_path()) fs::create_directories(p.parent_path());
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write " + p.string());
  out << content;
}

std::string read_stdin() { return std::string(std::istreambuf_iterator<char>(std::cin), {}); }

fs::path find_workspace(const fs::path& start) {
  for (const char* var : {"ORCH_WORKSPACE", "WS_ROOT"}) {
    if (const char* v = std::getenv(var); v && *v) return fs::canonical(v);
  }
  fs::path dir = fs::canonical(start);
  for (;;) {
    if (fs::exists(dir / "AGENTS.md") && fs::is_directory(dir / "_shared")) return dir;
    if (dir == dir.root_path()) throw std::runtime_error("no workspace found (looked for AGENTS.md + _shared/); set ORCH_WORKSPACE");
    dir = dir.parent_path();
  }
}

bool is_within(const fs::path& parent, const fs::path& child) {
  auto p = parent.lexically_normal();
  auto c = child.lexically_normal();
  auto rel = c.lexically_relative(p);
  if (rel.empty()) return false;
  auto first = *rel.begin();
  return first != ".." && !rel.is_absolute();
}

std::string trim(const std::string& s) {
  auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::string random_hex(size_t n) {
  static std::random_device rd;
  static const char* hex = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) s += hex[rd() % 16];
  return s;
}

std::string uuid_v4() {
  std::string h = random_hex(32);
  h[12] = '4';
  h[16] = "89ab"[std::random_device{}() % 4];
  return h.substr(0, 8) + "-" + h.substr(8, 4) + "-" + h.substr(12, 4) + "-" + h.substr(16, 4) + "-" + h.substr(20, 12);
}

std::string now_iso() {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
  std::ostringstream o;
  o << buf << '.' << (ms < 100 ? (ms < 10 ? "00" : "0") : "") << ms << 'Z';
  return o.str();
}

}  // namespace ws
