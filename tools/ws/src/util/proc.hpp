#pragma once
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ws {

struct RunResult {
  int code = -1;  // exit status; -1 if killed by a signal or failed to start
  bool timed_out = false;
  std::string out;
  std::string err;
};

struct RunOptions {
  std::string cwd;
  std::string input;
  int timeout_ms = 0;  // 0 = no timeout
  std::map<std::string, std::string> env;  // added to / overriding the parent environment
};

// Run argv[0] (resolved on PATH) to completion, feeding `input` on stdin.
RunResult run(const std::vector<std::string>& argv, const RunOptions& opts);

// Run and require exit 0; throws std::runtime_error with stderr otherwise. Returns trimmed stdout.
std::string run_ok(const std::vector<std::string>& argv, const std::string& cwd);

// A long-lived child with line-oriented stdio (used for JSON-RPC protocols).
class Child {
 public:
  Child(const std::vector<std::string>& argv, const std::string& cwd);
  ~Child();
  Child(const Child&) = delete;
  Child& operator=(const Child&) = delete;

  void write_line(const std::string& line);
  // Next complete stdout line, or nullopt on EOF or when the deadline passes (check timed_out()).
  std::optional<std::string> read_line(std::chrono::steady_clock::time_point deadline);
  bool timed_out() const { return timed_out_; }
  const std::string& stderr_text() const { return err_; }
  void close_stdin();
  int terminate();  // SIGTERM, then SIGKILL after a grace period; returns exit status

 private:
  void pump(int timeout_ms);
  int pid_ = -1;
  int in_ = -1, out_ = -1, errfd_ = -1;
  std::string buf_, err_;
  bool eof_ = false, timed_out_ = false, reaped_ = false;
  int status_ = -1;
};

}  // namespace ws
