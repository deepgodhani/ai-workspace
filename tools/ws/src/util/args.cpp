#include "util/args.hpp"

#include <stdexcept>

namespace ws {

Args::Args(const std::vector<std::string>& argv, const std::vector<OptSpec>& spec) {
  std::map<std::string, const OptSpec*> by_name;
  for (const auto& s : spec) by_name[s.name] = &s;
  for (size_t i = 0; i < argv.size(); ++i) {
    const std::string& a = argv[i];
    if (a == "-h") {
      flags_.insert("help");
      continue;
    }
    if (a.rfind("--", 0) != 0 || a == "--" || a == "-") {
      pos_.push_back(a);
      continue;
    }
    std::string name = a.substr(2), value;
    bool inline_value = false;
    if (auto eq = name.find('='); eq != std::string::npos) {
      value = name.substr(eq + 1);
      name = name.substr(0, eq);
      inline_value = true;
    }
    auto it = by_name.find(name);
    if (it == by_name.end()) throw std::runtime_error("unknown option --" + name);
    const OptSpec& s = *it->second;
    if (!s.takes_value) {
      if (inline_value) throw std::runtime_error("option --" + name + " takes no value");
      flags_.insert(name);
      continue;
    }
    if (!inline_value) {
      if (i + 1 >= argv.size()) throw std::runtime_error("option --" + name + " needs a value");
      value = argv[++i];
    }
    auto& v = values_[name];
    if (!s.multiple) v.clear();
    v.push_back(value);
  }
  for (const auto& s : spec) {
    if (s.takes_value && !s.default_value.empty() && !values_.count(s.name)) values_[s.name] = {s.default_value};
  }
}

std::optional<std::string> Args::get(const std::string& name) const {
  auto it = values_.find(name);
  if (it == values_.end() || it->second.empty()) return std::nullopt;
  return it->second.back();
}

std::string Args::get_or(const std::string& name, const std::string& fallback) const { return get(name).value_or(fallback); }

std::vector<std::string> Args::all(const std::string& name) const {
  auto it = values_.find(name);
  return it == values_.end() ? std::vector<std::string>{} : it->second;
}

}  // namespace ws
