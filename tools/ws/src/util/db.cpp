#include "util/db.hpp"

#include <stdexcept>

namespace ws {

Db::Db(const std::string& path) {
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
    sqlite3_close(db_);
    throw std::runtime_error("cannot open " + path + ": " + msg);
  }
  sqlite3_busy_timeout(db_, 5000);
}

Db::~Db() { sqlite3_close(db_); }

void Db::exec(const std::string& sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "unknown error";
    sqlite3_free(err);
    throw std::runtime_error("sqlite: " + msg);
  }
}

std::vector<Row> Db::query(const std::string& sql, const std::vector<Value>& params) {
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(std::string("sqlite prepare: ") + sqlite3_errmsg(db_));
  for (size_t i = 0; i < params.size(); ++i) {
    int idx = static_cast<int>(i) + 1;
    const Value& v = params[i];
    if (std::holds_alternative<std::monostate>(v)) sqlite3_bind_null(st, idx);
    else if (auto p = std::get_if<long long>(&v)) sqlite3_bind_int64(st, idx, *p);
    else if (auto p = std::get_if<double>(&v)) sqlite3_bind_double(st, idx, *p);
    else sqlite3_bind_text(st, idx, std::get<std::string>(v).c_str(), -1, SQLITE_TRANSIENT);
  }
  std::vector<Row> rows;
  int rc;
  while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
    Row r;
    for (int c = 0; c < sqlite3_column_count(st); ++c) {
      std::string name = sqlite3_column_name(st, c);
      switch (sqlite3_column_type(st, c)) {
        case SQLITE_INTEGER: r[name] = static_cast<long long>(sqlite3_column_int64(st, c)); break;
        case SQLITE_FLOAT: r[name] = sqlite3_column_double(st, c); break;
        case SQLITE_NULL: r[name] = std::monostate{}; break;
        default: r[name] = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, c))); break;
      }
    }
    rows.push_back(std::move(r));
  }
  std::string err = rc == SQLITE_DONE ? "" : sqlite3_errmsg(db_);
  sqlite3_finalize(st);
  if (!err.empty()) throw std::runtime_error("sqlite step: " + err);
  return rows;
}

void Db::run(const std::string& sql, const std::vector<Value>& params) { query(sql, params); }

std::optional<std::string> as_text(const Row& r, const std::string& col) {
  auto it = r.find(col);
  if (it == r.end()) return std::nullopt;
  if (auto s = std::get_if<std::string>(&it->second)) return *s;
  if (auto i = std::get_if<long long>(&it->second)) return std::to_string(*i);
  return std::nullopt;
}

long long as_int(const Row& r, const std::string& col, long long fallback) {
  auto it = r.find(col);
  if (it == r.end()) return fallback;
  if (auto i = std::get_if<long long>(&it->second)) return *i;
  if (auto d = std::get_if<double>(&it->second)) return static_cast<long long>(*d);
  return fallback;
}

std::optional<double> as_real(const Row& r, const std::string& col) {
  auto it = r.find(col);
  if (it == r.end()) return std::nullopt;
  if (auto d = std::get_if<double>(&it->second)) return *d;
  if (auto i = std::get_if<long long>(&it->second)) return static_cast<double>(*i);
  return std::nullopt;
}

Value nullable(const std::optional<std::string>& s) { return s ? Value{*s} : Value{std::monostate{}}; }

}  // namespace ws
