#include <sqlite3.h>

#include <iostream>
#include <nlohmann/json.hpp>

#include "commands.hpp"

namespace ws {
int cmd_version(const std::vector<std::string>&) {
  std::cout << "ws 0.1.0 (sqlite " << sqlite3_libversion() << ", nlohmann/json " << NLOHMANN_JSON_VERSION_MAJOR << "."
            << NLOHMANN_JSON_VERSION_MINOR << "." << NLOHMANN_JSON_VERSION_PATCH << ")\n";
  return 0;
}
}  // namespace ws
