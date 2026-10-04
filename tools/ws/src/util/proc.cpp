#include "util/proc.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

extern char** environ;

namespace ws {
namespace {

struct Pipe {
  int r = -1, w = -1;
  Pipe() {
    int fds[2];
    if (pipe(fds) != 0) throw std::runtime_error(std::string("pipe: ") + std::strerror(errno));
    r = fds[0];
    w = fds[1];
    fcntl(r, F_SETFD, FD_CLOEXEC);
    fcntl(w, F_SETFD, FD_CLOEXEC);
  }
};

void close_fd(int& fd) {
  if (fd >= 0) close(fd);
  fd = -1;
}

int decode_status(int status) {
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return -1;
}

std::vector<std::string> make_env(const std::map<std::string, std::string>& extra) {
  std::vector<std::string> env;
  for (char** e = environ; e && *e; ++e) {
    std::string kv(*e);
    auto key = kv.substr(0, kv.find('='));
    if (!extra.count(key)) env.push_back(kv);
  }
  for (const auto& [k, v] : extra) env.push_back(k + "=" + v);
  return env;
}

// Spawn with stdin/stdout/stderr connected to the given pipe ends.
int spawn_child(const std::vector<std::string>& argv, const std::string& cwd, const std::map<std::string, std::string>& extra_env,
                int in_r, int out_w, int err_w) {
  if (argv.empty()) throw std::runtime_error("spawn: empty argv");
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, in_r, 0);
  posix_spawn_file_actions_adddup2(&fa, out_w, 1);
  posix_spawn_file_actions_adddup2(&fa, err_w, 2);
  if (!cwd.empty()) {
#if defined(__APPLE__)
    if (__builtin_available(macOS 26.0, *)) {
      posix_spawn_file_actions_addchdir(&fa, cwd.c_str());
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      posix_spawn_file_actions_addchdir_np(&fa, cwd.c_str());
#pragma clang diagnostic pop
    }
#else
    posix_spawn_file_actions_addchdir_np(&fa, cwd.c_str());
#endif
  }

  std::vector<char*> args;
  for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);
  auto env = make_env(extra_env);
  std::vector<char*> envp;
  for (auto& e : env) envp.push_back(e.data());
  envp.push_back(nullptr);

  pid_t pid = -1;
  int rc = posix_spawnp(&pid, argv[0].c_str(), &fa, nullptr, args.data(), envp.data());
  posix_spawn_file_actions_destroy(&fa);
  if (rc != 0) throw std::runtime_error("failed to start " + argv[0] + ": " + std::strerror(rc));
  return pid;
}

int kill_and_reap(int pid) {
  kill(pid, SIGTERM);
  int status = 0;
  for (int i = 0; i < 50; ++i) {
    if (waitpid(pid, &status, WNOHANG) == pid) return decode_status(status);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  kill(pid, SIGKILL);
  waitpid(pid, &status, 0);
  return decode_status(status);
}

}  // namespace

RunResult run(const std::vector<std::string>& argv, const RunOptions& opts) {
  Pipe in, out, err;
  int pid = spawn_child(argv, opts.cwd, opts.env, in.r, out.w, err.w);
  close_fd(in.r);
  close_fd(out.w);
  close_fd(err.w);
  fcntl(in.w, F_SETFL, O_NONBLOCK);

  RunResult res;
  size_t written = 0;
  if (opts.input.empty()) close_fd(in.w);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(opts.timeout_ms);

  while (out.r >= 0 || err.r >= 0) {
    std::vector<pollfd> fds;
    if (out.r >= 0) fds.push_back({out.r, POLLIN, 0});
    if (err.r >= 0) fds.push_back({err.r, POLLIN, 0});
    if (in.w >= 0) fds.push_back({in.w, POLLOUT, 0});
    int wait_ms = 200;
    if (opts.timeout_ms > 0) {
      auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
      if (left <= 0) {
        res.timed_out = true;
        break;
      }
      wait_ms = static_cast<int>(std::min<long long>(left, 200));
    }
    if (poll(fds.data(), fds.size(), wait_ms) < 0 && errno != EINTR) break;
    for (auto& p : fds) {
      if (!p.revents) continue;
      if (p.fd == in.w) {
        ssize_t n = write(in.w, opts.input.data() + written, opts.input.size() - written);
        if (n > 0) written += static_cast<size_t>(n);
        if (n < 0 && errno != EAGAIN) written = opts.input.size();
        if (written >= opts.input.size()) close_fd(in.w);
        continue;
      }
      char buf[65536];
      ssize_t n = read(p.fd, buf, sizeof buf);
      if (n > 0) {
        (p.fd == out.r ? res.out : res.err).append(buf, static_cast<size_t>(n));
      } else if (n == 0 || (errno != EAGAIN && errno != EINTR)) {
        if (p.fd == out.r) close_fd(out.r); else close_fd(err.r);
      }
    }
  }
  close_fd(in.w);
  close_fd(out.r);
  close_fd(err.r);
  if (res.timed_out) {
    res.code = kill_and_reap(pid);
  } else {
    int status = 0;
    waitpid(pid, &status, 0);
    res.code = decode_status(status);
  }
  return res;
}

int run_inherit(const std::vector<std::string>& argv, bool discard_stderr, bool discard_stdout) {
  if (argv.empty()) throw std::runtime_error("spawn: empty argv");
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  if (discard_stdout) posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
  if (discard_stderr) posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
  // main() ignores SIGPIPE; a shell's child would see the default action.
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  sigset_t def;
  sigemptyset(&def);
  sigaddset(&def, SIGPIPE);
  posix_spawnattr_setsigdefault(&attr, &def);
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF);

  std::vector<char*> args;
  for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);
  pid_t pid = -1;
  int rc = posix_spawnp(&pid, argv[0].c_str(), &fa, &attr, args.data(), environ);
  posix_spawn_file_actions_destroy(&fa);
  posix_spawnattr_destroy(&attr);
  if (rc != 0) throw std::runtime_error("failed to start " + argv[0] + ": " + std::strerror(rc));
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
  return decode_status(status);
}

RunResult run_capture_stdout(const std::vector<std::string>& argv) {
  Pipe out;
  int pid = spawn_child(argv, "", {}, 0, out.w, 2);  // stdin and stderr inherited, as in `$(…)`
  close_fd(out.w);
  RunResult res;
  char buf[65536];
  for (;;) {
    ssize_t n = read(out.r, buf, sizeof buf);
    if (n > 0) res.out.append(buf, static_cast<size_t>(n));
    else if (n == 0 || errno != EINTR) break;
  }
  close_fd(out.r);
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
  res.code = decode_status(status);
  return res;
}

std::optional<std::string> find_on_path(const std::string& name) {
  const char* path = std::getenv("PATH");
  if (!path) return std::nullopt;
  std::string p = path;
  size_t start = 0;
  for (;;) {
    size_t end = p.find(':', start);
    std::string dir = p.substr(start, end == std::string::npos ? std::string::npos : end - start);
    std::string cand = (dir.empty() ? std::string(".") : dir) + "/" + name;
    struct stat st{};
    if (stat(cand.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(cand.c_str(), X_OK) == 0) return cand;
    if (end == std::string::npos) return std::nullopt;
    start = end + 1;
  }
}

std::string run_ok(const std::vector<std::string>& argv, const std::string& cwd) {
  auto r = run(argv, RunOptions{cwd, "", 0, {}});
  if (r.code != 0) {
    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "" : " ") + a;
    throw std::runtime_error(cmd + " failed (" + std::to_string(r.code) + "): " + r.err);
  }
  auto s = r.out;
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return s;
}

Child::Child(const std::vector<std::string>& argv, const std::string& cwd) {
  Pipe in, out, err;
  pid_ = spawn_child(argv, cwd, {}, in.r, out.w, err.w);
  close(in.r);
  close(out.w);
  close(err.w);
  in_ = in.w;
  out_ = out.r;
  errfd_ = err.r;
  fcntl(errfd_, F_SETFL, O_NONBLOCK);
}

Child::~Child() {
  close_stdin();
  if (!reaped_) terminate();
  close_fd(out_);
  close_fd(errfd_);
}

void Child::write_line(const std::string& line) {
  if (in_ < 0) return;
  std::string data = line + "\n";
  size_t off = 0;
  while (off < data.size()) {
    ssize_t n = write(in_, data.data() + off, data.size() - off);
    if (n < 0) {
      if (errno == EINTR) continue;
      close_stdin();
      return;
    }
    off += static_cast<size_t>(n);
  }
}

void Child::pump(int timeout_ms) {
  std::vector<pollfd> fds;
  if (out_ >= 0 && !eof_) fds.push_back({out_, POLLIN, 0});
  if (errfd_ >= 0) fds.push_back({errfd_, POLLIN, 0});
  if (fds.empty()) return;
  if (poll(fds.data(), fds.size(), timeout_ms) <= 0) return;
  for (auto& p : fds) {
    if (!p.revents) continue;
    char b[65536];
    ssize_t n = read(p.fd, b, sizeof b);
    if (p.fd == out_) {
      if (n > 0) buf_.append(b, static_cast<size_t>(n));
      else if (n == 0 || (errno != EAGAIN && errno != EINTR)) eof_ = true;
    } else {
      if (n > 0) err_.append(b, static_cast<size_t>(n));
      else if (n == 0) close_fd(errfd_);
    }
  }
}

std::optional<std::string> Child::read_line(std::chrono::steady_clock::time_point deadline) {
  for (;;) {
    auto nl = buf_.find('\n');
    if (nl != std::string::npos) {
      std::string line = buf_.substr(0, nl);
      buf_.erase(0, nl + 1);
      return line;
    }
    if (eof_) return std::nullopt;
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    if (left <= 0) {
      timed_out_ = true;
      return std::nullopt;
    }
    pump(static_cast<int>(std::min<long long>(left, 200)));
  }
}

void Child::close_stdin() { close_fd(in_); }

int Child::terminate() {
  if (reaped_) return status_;
  status_ = kill_and_reap(pid_);
  reaped_ = true;
  return status_;
}

}  // namespace ws
