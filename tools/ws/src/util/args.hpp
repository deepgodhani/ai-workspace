#pragma once
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ws {

struct OptSpec {
  std::string name;
  bool takes_value = false;
  bool multiple = false;
  std::string default_value;
  OptSpec(std::string n, bool value = false, bool many = false, std::string dflt = {})
      : name(std::move(n)), takes_value(value), multiple(many), default_value(std::move(dflt)) {}
};

class Args {
 public:
  // Strict: unknown options throw. Accepts --name value, --name=value, and -h for --help.
  Args(const std::vector<std::string>& argv, const std::vector<OptSpec>& spec);

  bool flag(const std::string& name) const { return flags_.count(name) > 0; }
  std::optional<std::string> get(const std::string& name) const;
  std::string get_or(const std::string& name, const std::string& fallback) const;
  std::vector<std::string> all(const std::string& name) const;
  const std::vector<std::string>& positionals() const { return pos_; }

 private:
  std::set<std::string> flags_;
  std::map<std::string, std::vector<std::string>> values_;
  std::vector<std::string> pos_;
};

}  // namespace ws
