#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "util/db.hpp"

namespace ws::orch {

struct NewSession {
  std::string id, cli, target, cwd, mode, task;
  std::optional<std::string> cli_session_id, parent_id, worktree, branch, base, merged_from, allow_json, model, sandbox;
  long long full = 0;
};

struct Finish {
  std::string status;
  std::optional<long long> exit_code;
  std::optional<std::string> cli_session_id;
  std::optional<double> cost_usd, credits;
  long long input_tokens = 0, output_tokens = 0, cache_read_tokens = 0;
  std::optional<std::string> result_json, error;
};

class Registry {
 public:
  explicit Registry(const std::string& workspace);
  void insert(const NewSession& s);
  void finish(const std::string& id, const Finish& f);
  std::optional<Row> get(const std::string& id_or_prefix);
  std::vector<Row> list(bool all);
  void close(const std::string& id);

 private:
  std::unique_ptr<Db> db_;
};

}  // namespace ws::orch
