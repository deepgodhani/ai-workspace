#pragma once
#include <sqlite3.h>

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ws {

using Value = std::variant<std::monostate, long long, double, std::string>;
using Row = std::map<std::string, Value>;

class Db {
 public:
  explicit Db(const std::string& path);
  ~Db();
  Db(const Db&) = delete;
  Db& operator=(const Db&) = delete;

  void exec(const std::string& sql);
  std::vector<Row> query(const std::string& sql, const std::vector<Value>& params = {});
  void run(const std::string& sql, const std::vector<Value>& params = {});

 private:
  sqlite3* db_ = nullptr;
};

std::optional<std::string> as_text(const Row& r, const std::string& col);
long long as_int(const Row& r, const std::string& col, long long fallback = 0);
std::optional<double> as_real(const Row& r, const std::string& col);
Value nullable(const std::optional<std::string>& s);

}  // namespace ws
