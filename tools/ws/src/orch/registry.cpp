#include "orch/registry.hpp"

#include <set>
#include <stdexcept>

#include "util/fs.hpp"

namespace ws::orch {

namespace {
Value opt_real(const std::optional<double>& v) { return v ? Value{*v} : Value{std::monostate{}}; }
Value opt_int(const std::optional<long long>& v) { return v ? Value{*v} : Value{std::monostate{}}; }
}  // namespace

Registry::Registry(const std::string& workspace) {
  fs::path dir = fs::path(workspace) / ".orch";
  fs::create_directories(dir);
  db_ = std::make_unique<Db>((dir / "orch.db").string());
  db_->exec(R"(create table if not exists sessions (
      id text primary key, cli text not null, cli_session_id text, parent_id text,
      target text not null, cwd text not null, mode text not null,
      worktree text, branch text, base text, task text not null,
      status text not null, started_at text not null, ended_at text,
      exit_code integer, cost_usd real, input_tokens integer default 0,
      output_tokens integer default 0, cache_read_tokens integer default 0,
      result_json text, error text, closed integer default 0))");
  std::set<std::string> have;
  for (const auto& r : db_->query("pragma table_info(sessions)")) have.insert(*as_text(r, "name"));
  const std::vector<std::pair<std::string, std::string>> added = {
      {"merged_from", "text"}, {"allow_json", "text"}, {"full", "integer default 0"}, {"model", "text"}, {"credits", "real"}, {"sandbox", "text"}};
  for (const auto& [col, type] : added)
    if (!have.count(col)) db_->exec("alter table sessions add column " + col + " " + type);
}

void Registry::insert(const NewSession& s) {
  db_->run(R"(insert into sessions (id, cli, cli_session_id, parent_id, target, cwd, mode, worktree, branch, base, task, status, started_at,
              merged_from, allow_json, full, model, sandbox)
              values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'running', ?, ?, ?, ?, ?, ?))",
           {s.id, s.cli, nullable(s.cli_session_id), nullable(s.parent_id), s.target, s.cwd, s.mode, nullable(s.worktree), nullable(s.branch),
            nullable(s.base), s.task, now_iso(), nullable(s.merged_from), nullable(s.allow_json), s.full, nullable(s.model), nullable(s.sandbox)});
}

void Registry::finish(const std::string& id, const Finish& f) {
  db_->run(R"(update sessions set status=?, exit_code=?, cli_session_id=coalesce(?, cli_session_id), cost_usd=?, input_tokens=?, output_tokens=?,
              cache_read_tokens=?, result_json=?, error=?, credits=?, ended_at=? where id=?)",
           {f.status, opt_int(f.exit_code), nullable(f.cli_session_id), opt_real(f.cost_usd), f.input_tokens, f.output_tokens, f.cache_read_tokens,
            nullable(f.result_json), nullable(f.error), opt_real(f.credits), now_iso(), id});
}

std::optional<Row> Registry::get(const std::string& id) {
  auto rows = db_->query("select * from sessions where id = ? or id like ? order by started_at", {id, id + "%"});
  if (rows.size() > 1) {
    std::string ids;
    for (const auto& r : rows) ids += (ids.empty() ? "" : ", ") + *as_text(r, "id");
    throw std::runtime_error("ambiguous id prefix '" + id + "': " + ids);
  }
  if (rows.empty()) return std::nullopt;
  return rows[0];
}

std::vector<Row> Registry::list(bool all) {
  return db_->query(std::string("select * from sessions ") + (all ? "" : "where closed = 0") + " order by started_at desc limit 50");
}

void Registry::close(const std::string& id) { db_->run("update sessions set closed = 1 where id = ?", {id}); }

}  // namespace ws::orch
