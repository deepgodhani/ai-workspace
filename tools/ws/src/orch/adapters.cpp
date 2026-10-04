#include "orch/adapters.hpp"

#include <cstdlib>
#include <sstream>

#include "orch/packet.hpp"
#include "orch/sandbox.hpp"
#include "util/fs.hpp"

namespace ws::orch {

using json = nlohmann::json;

std::string format_number(double v) {
  std::ostringstream o;
  o << v;
  return o.str();
}

namespace {
std::string bin(const char* env, const char* fallback) {
  const char* v = std::getenv(env);
  return v && *v ? v : fallback;
}
long long num(const json& j, const char* k) { return j.contains(k) && j[k].is_number() ? j[k].get<long long>() : 0; }
}  // namespace

Invocation claude_invocation(const SpawnOpts& o) {
  std::vector<std::string> a = {bin("ORCH_CLAUDE_BIN", "claude"), "-p", "--output-format", "json", "--json-schema", result_schema().dump(), "--permission-prompts", "none"};
  if (o.resume_id) a.insert(a.end(), {"--resume", *o.resume_id});
  else if (o.session_id) a.insert(a.end(), {"--session-id", *o.session_id});
  if (o.mode == "read") {
    a.insert(a.end(), {"--tools", "Read", "Grep", "Glob"});
  } else {
    a.insert(a.end(), {"--permission-mode", "acceptEdits"});
    if (!o.allow.empty()) {
      a.push_back("--allowedTools");
      a.insert(a.end(), o.allow.begin(), o.allow.end());
    }
  }
  if (!o.full) a.insert(a.end(), {"--disable-slash-commands", "--strict-mcp-config", "--exclude-dynamic-system-prompt-sections"});
  if (o.sandbox == "native") a.insert(a.end(), {"--settings", claude_native_settings()});
  if (o.model) a.insert(a.end(), {"--model", *o.model});
  if (o.budget_usd) a.insert(a.end(), {"--max-budget-usd", format_number(*o.budget_usd)});
  return {a, o.prompt};
}

Invocation codex_invocation(const SpawnOpts& o) {
  std::string sandbox = o.mode == "read" ? "read-only" : "workspace-write";
  std::vector<std::string> common = {"--json", "--output-schema", o.schema_file, "-o", o.last_message_file};
  if (o.model) common.insert(common.end(), {"-m", *o.model});
  std::vector<std::string> a = {bin("ORCH_CODEX_BIN", "codex"), "exec"};
  if (o.resume_id) {
    a.push_back("resume");
    a.insert(a.end(), common.begin(), common.end());
    a.insert(a.end(), {"-c", "sandbox_mode=\"" + sandbox + "\"", *o.resume_id, "-"});
  } else {
    a.insert(a.end(), common.begin(), common.end());
    a.insert(a.end(), {"-C", o.cwd, "-s", sandbox, "-"});
  }
  return {a, o.prompt};
}

ChildOutput parse_claude(const std::string& stdout_text) {
  ChildOutput out;
  std::string last;
  std::istringstream lines(trim(stdout_text));
  for (std::string l; std::getline(lines, l);)
    if (!trim(l).empty()) last = l;
  json j = json::parse(last, nullptr, false);
  if (j.is_discarded() || !j.is_object()) {
    out.error = "unparseable claude output: " + stdout_text.substr(0, 200);
    return out;
  }
  if (j.contains("session_id") && j["session_id"].is_string()) out.cli_session_id = j["session_id"].get<std::string>();
  if (j.contains("total_cost_usd") && j["total_cost_usd"].is_number()) out.cost_usd = j["total_cost_usd"].get<double>();
  json u = j.value("usage", json::object());
  out.input_tokens = num(u, "input_tokens") + num(u, "cache_creation_input_tokens");
  out.cache_read_tokens = num(u, "cache_read_input_tokens");
  out.output_tokens = num(u, "output_tokens");
  json structured = j.contains("structured_output") ? j["structured_output"] : j.value("structuredOutput", json());
  if (is_result(structured)) out.result = structured;
  else if (j.contains("result") && j["result"].is_string()) out.result = parse_result(j["result"].get<std::string>());
  if (j.value("is_error", false)) {
    if (j.contains("result") && !j["result"].is_null()) out.error = j["result"].is_string() ? j["result"].get<std::string>() : j["result"].dump();
    else out.error = j.value("subtype", std::string("claude reported an error"));
  } else if (!out.result) {
    out.error = "no schema-valid result (subtype=" + j.value("subtype", std::string("?")) + ")";
  }
  return out;
}

ChildOutput parse_codex(const std::string& stdout_text, const std::string& last_message) {
  ChildOutput out;
  std::istringstream lines(stdout_text);
  for (std::string line; std::getline(lines, line);) {
    if (trim(line).rfind('{', 0) != 0) continue;
    json e = json::parse(line, nullptr, false);
    if (e.is_discarded()) continue;
    std::string type = e.value("type", "");
    if (type == "thread.started" && e.contains("thread_id")) out.cli_session_id = e["thread_id"].get<std::string>();
    if (type == "turn.completed" && e.contains("usage")) {
      long long cached = num(e["usage"], "cached_input_tokens");
      out.cache_read_tokens += cached;
      out.input_tokens += num(e["usage"], "input_tokens") - cached;
      out.output_tokens += num(e["usage"], "output_tokens");
    }
    if ((type == "turn.failed" || type == "error") && !out.error) {
      out.error = e.contains("error") ? e["error"].dump() : e.contains("message") ? e["message"].dump() : e.dump();
    }
  }
  out.result = parse_result(last_message);
  if (!out.result && !out.error) out.error = "no schema-valid result in codex last message";
  return out;
}

}  // namespace ws::orch
