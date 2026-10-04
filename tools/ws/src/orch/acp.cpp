#include "orch/acp.hpp"

#include <chrono>
#include <iterator>
#include <cstdlib>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <stdexcept>

#include "orch/packet.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws::orch {

using json = nlohmann::json;

const char* JSON_INSTRUCTION_PREFIX = "\n\nReply with ONLY one JSON object (no prose, no code fence) matching this JSON Schema:\n";
namespace {
const char* REPAIR_PROMPT = "Your last reply was not a JSON object matching the schema. Reply again with ONLY that JSON object, nothing else.";

bool inside(const std::string& cwd, const std::string& p) {
  fs::path base = fs::path(cwd).lexically_normal();
  fs::path abs = fs::path(p).is_absolute() ? fs::path(p) : fs::path(cwd) / p;
  fs::path rel = abs.lexically_normal().lexically_relative(base);
  if (rel.empty()) return false;
  if (rel == ".") return true;
  return *rel.begin() != ".." && !rel.is_absolute();
}

std::string join(const std::vector<std::string>& v, const std::string& sep) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) out += (i ? sep : "") + v[i];
  return out;
}
}  // namespace

std::string json_instruction() { return std::string(JSON_INSTRUCTION_PREFIX) + result_schema().dump(); }

namespace {
bool segment_allowed(const std::string& cmd, const std::vector<std::string>& allow) {
  static const std::regex wrapped("^Bash\\((.*)\\)$");
  for (const auto& a : allow) {
    std::smatch m;
    std::string pat = trim(std::regex_match(a, m, wrapped) ? m[1].str() : a);
    if (pat.size() >= 2 && pat.compare(pat.size() - 2, 2, ":*") == 0) {
      std::string prefix = trim(pat.substr(0, pat.size() - 2));
      if (cmd == prefix || cmd.rfind(prefix + " ", 0) == 0) return true;
    } else if (cmd == pat) {
      return true;
    }
  }
  return false;
}
}  // namespace

bool command_allowed(const std::string& command, const std::vector<std::string>& allow) {
  // Substitution and redirection can run or write anything: never allowed.
  for (const char* bad : {"`", "$(", "<(", ">(", ">", "<", "\n", "\r"})
    if (command.find(bad) != std::string::npos) return false;
  // Chained commands: every segment must match an allow pattern on its own.
  static const std::regex sep("&&|\\|\\||;|\\||&");
  // std::sregex_token_iterator drops a trailing empty segment, so count separators
  // and require exactly separators + 1 non-empty, allowed segments.
  auto separators = std::distance(std::sregex_iterator(command.begin(), command.end(), sep), std::sregex_iterator());
  std::sregex_token_iterator it(command.begin(), command.end(), sep, -1), end;
  long segments = 0;
  for (; it != end; ++it) {
    std::string seg = trim(it->str());
    if (seg.empty() || !segment_allowed(seg, allow)) return false;
    ++segments;
  }
  return segments > 0 && segments == separators + 1;
}

Decision decide(const PermissionRequest& req, const std::string& mode, const std::string& cwd, const std::vector<std::string>& allow) {
  if (mode == "read") return {false, "read-only session"};
  std::string kind = req.kind.value_or(req.command && req.paths.empty() ? "execute" : !req.paths.empty() ? "edit" : "other");
  if (kind == "edit" || kind == "delete" || kind == "move") {
    if (req.paths.empty()) return {false, kind + " without a path"};
    std::vector<std::string> outside;
    for (const auto& p : req.paths)
      if (!inside(cwd, p)) outside.push_back(p);
    if (!outside.empty()) return {false, "outside worktree: " + join(outside, ", ")};
    return {true, kind + " inside worktree"};
  }
  if (kind == "execute") {
    if (req.command && command_allowed(*req.command, allow)) return {true, "command matches --allow"};
    return {false, "command not in --allow: " + req.command.value_or("?")};
  }
  return {false, "tool kind '" + kind + "' not permitted"};
}

std::vector<std::string> acp_command(const std::optional<std::string>& model) {
  const char* b = std::getenv("ORCH_KIRO_BIN");
  std::vector<std::string> a = {b && *b ? b : "kiro-cli", "acp"};
  if (model) a.insert(a.end(), {"--model", *model});
  return a;
}

AcpOutput run_acp(const AcpRun& o) {
  AcpOutput out;
  std::vector<std::string> argv = o.wrapper;
  auto base = acp_command(o.model);
  argv.insert(argv.end(), base.begin(), base.end());
  std::ofstream log(o.log_file, std::ios::app);
  auto log_msg = [&](const char* dir, const json& m) { log << json{{"dir", dir}, {"m", m}}.dump() << "\n" << std::flush; };
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(o.timeout_ms);

  std::unique_ptr<Child> child;
  try {
    child = std::make_unique<Child>(argv, o.cwd);
  } catch (const std::exception& e) {
    out.error = e.what();
    return out;
  }
  std::map<std::string, std::string> kinds;
  bool collecting = false;
  std::string text;
  long long next_id = 0;

  auto send = [&](const json& m) {
    log_msg("out", m);
    child->write_line(m.dump());
  };

  auto handle = [&](const json& m) {
    std::string method = m.value("method", "");
    if (method == "session/request_permission") {
      json params = m.value("params", json::object());
      json tc = params.value("toolCall", json::object());
      json raw = tc.value("rawInput", json::object());
      PermissionRequest req;
      if (tc.contains("kind") && tc["kind"].is_string()) req.kind = tc["kind"].get<std::string>();
      else if (tc.contains("toolCallId") && kinds.count(tc["toolCallId"].get<std::string>())) req.kind = kinds[tc["toolCallId"].get<std::string>()];
      req.title = tc.value("title", "");
      if (raw.contains("path") && raw["path"].is_string()) req.paths.push_back(raw["path"].get<std::string>());
      for (const auto& l : tc.value("locations", json::array()))
        if (l.contains("path") && l["path"].is_string()) req.paths.push_back(l["path"].get<std::string>());
      if (raw.contains("command") && raw["command"].is_string() && !raw.contains("path")) req.command = raw["command"].get<std::string>();
      Decision d = decide(req, o.mode, o.cwd, o.allow);
      std::string want = d.allow ? "allow_once" : "reject_once", family = d.allow ? "allow" : "reject";
      std::optional<std::string> pick;
      for (const auto& opt : params.value("options", json::array()))
        if (opt.value("kind", "") == want) { pick = opt.value("optionId", ""); break; }
      if (!pick)
        for (const auto& opt : params.value("options", json::array()))
          if (opt.value("kind", "").rfind(family, 0) == 0) { pick = opt.value("optionId", ""); break; }
      if (!d.allow) out.denied.push_back(req.title + " (" + d.reason + ")");
      json outcome = pick ? json{{"outcome", "selected"}, {"optionId", *pick}} : json{{"outcome", "cancelled"}};
      send({{"jsonrpc", "2.0"}, {"id", m["id"]}, {"result", {{"outcome", outcome}}}});
    } else if (m.contains("id") && !method.empty()) {
      send({{"jsonrpc", "2.0"}, {"id", m["id"]}, {"error", {{"code", -32601}, {"message", "orch does not support " + method}}}});
    } else if (method == "session/update") {
      json u = m.value("params", json::object()).value("update", json::object());
      std::string su = u.value("sessionUpdate", "");
      if (su == "tool_call" && u.contains("toolCallId")) kinds[u["toolCallId"].get<std::string>()] = u.value("kind", "");
      if (collecting && su == "agent_message_chunk" && u.contains("content") && u["content"].value("type", "") == "text")
        text += u["content"].value("text", "");
    } else if (method == "_kiro.dev/metadata") {
      for (const auto& mu : m.value("params", json::object()).value("meteringUsage", json::array()))
        if (mu.value("unit", "") == "credit" && mu.contains("value") && mu["value"].is_number()) out.credits += mu["value"].get<double>();
    }
  };

  auto call = [&](const std::string& method, const json& params) -> json {
    long long id = next_id++;
    send({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}});
    for (;;) {
      auto line = child->read_line(deadline);
      if (!line) {
        if (child->timed_out()) {
          out.timed_out = true;
          throw std::runtime_error("timeout");
        }
        throw std::runtime_error(method + " failed: agent exited before replying");
      }
      if (trim(*line).empty()) continue;
      json m = json::parse(*line, nullptr, false);
      if (m.is_discarded()) continue;
      log_msg("in", m);
      if (m.contains("id") && !m.contains("method") && m["id"].is_number() && m["id"].get<long long>() == id) {
        if (m.contains("error")) throw std::runtime_error(method + " failed: " + m["error"].value("message", m["error"].dump()));
        return m.value("result", json::object());
      }
      handle(m);
    }
  };

  try {
    call("initialize", {{"protocolVersion", 1},
                        {"clientCapabilities", {{"fs", {{"readTextFile", false}, {"writeTextFile", false}}}, {"terminal", false}}},
                        {"clientInfo", {{"name", "orch"}, {"version", "0.1.0"}}}});
    std::string session_id;
    if (o.resume_id) {
      call("session/load", {{"sessionId", *o.resume_id}, {"cwd", o.cwd}, {"mcpServers", json::array()}});
      session_id = *o.resume_id;
    } else {
      json s = call("session/new", {{"cwd", o.cwd}, {"mcpServers", json::array()}});
      if (!s.contains("sessionId")) throw std::runtime_error("session/new failed: no sessionId");
      session_id = s["sessionId"].get<std::string>();
    }
    out.cli_session_id = session_id;
    for (const std::string& prompt_text : {o.prompt + json_instruction(), std::string(REPAIR_PROMPT)}) {
      text.clear();
      collecting = true;
      json p = call("session/prompt", {{"sessionId", session_id}, {"prompt", json::array({{{"type", "text"}, {"text", prompt_text}}})}});
      collecting = false;
      std::string stop = p.value("stopReason", "");
      if (!stop.empty() && stop != "end_turn") throw std::runtime_error("turn ended with " + stop);
      out.result = parse_result(text);
      if (out.result) break;
    }
    if (!out.result) out.error = "no schema-valid result after one repair prompt; last reply: " + text.substr(0, 200);
  } catch (const std::exception& e) {
    out.error = out.timed_out ? "timed out after " + std::to_string(o.timeout_ms / 1000) + "s" : e.what();
    std::string err = trim(child->stderr_text());
    if (!err.empty()) *out.error += " | stderr: " + (err.size() > 300 ? err.substr(err.size() - 300) : err);
  }
  child->close_stdin();
  child->terminate();
  return out;
}

}  // namespace ws::orch
