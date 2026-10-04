#include "orch/packet.hpp"

#include <regex>
#include <set>

#include "util/fs.hpp"

namespace ws::orch {

namespace {
const std::vector<std::string> STARTUP_FILES = {".ai/STATE.md", ".ai/TASKS.md", ".ai/INDEX.md", "STATE.md", "TASKS.md", "INDEX.md", "CONTEXT.md", "STATUS.md"};

std::string join(const std::vector<std::string>& v, const std::string& sep) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) out += (i ? sep : "") + v[i];
  return out;
}

std::optional<json> try_parse(const std::string& s) {
  auto v = json::parse(s, nullptr, false);
  if (v.is_discarded() || !is_result(v)) return std::nullopt;
  return v;
}
}  // namespace

const json& result_schema() {
  static const json schema = json::parse(R"({
    "type": "object", "additionalProperties": false,
    "required": ["status", "summary", "files_changed", "decisions", "checks", "open_issues", "next_action"],
    "properties": {
      "status": {"type": "string", "enum": ["done", "partial", "blocked", "failed"]},
      "summary": {"type": "string"},
      "files_changed": {"type": "array", "items": {"type": "object", "additionalProperties": false, "required": ["path", "change"],
        "properties": {"path": {"type": "string"}, "change": {"type": "string"}}}},
      "decisions": {"type": "array", "items": {"type": "string"}},
      "checks": {"type": "array", "items": {"type": "object", "additionalProperties": false, "required": ["command", "result"],
        "properties": {"command": {"type": "string"}, "result": {"type": "string", "enum": ["pass", "fail", "not_run"]}}}},
      "open_issues": {"type": "array", "items": {"type": "string"}},
      "next_action": {"type": "string"}
    }})");
  return schema;
}

bool is_result(const json& v) {
  static const std::set<std::string> statuses = {"done", "partial", "blocked", "failed"};
  if (!v.is_object()) return false;
  auto has_string = [&](const char* k) { return v.contains(k) && v[k].is_string(); };
  auto has_array = [&](const char* k) { return v.contains(k) && v[k].is_array(); };
  return has_string("status") && statuses.count(v["status"].get<std::string>()) && has_string("summary") && has_array("files_changed") &&
         has_array("decisions") && has_array("checks") && has_array("open_issues") && has_string("next_action");
}

std::optional<json> parse_result(const std::string& text) {
  if (auto d = try_parse(trim(text))) return d;
  static const std::regex fence("```(?:json)?\\s*\\n([\\s\\S]*?)\\n```");
  std::smatch m;
  if (std::regex_search(text, m, fence)) {
    if (auto f = try_parse(m[1].str())) return f;
  }
  auto start = text.find('{');
  auto end = text.rfind('}');
  if (start != std::string::npos && end != std::string::npos && end > start) return try_parse(text.substr(start, end - start + 1));
  return std::nullopt;
}

std::string render_result(const std::string& id, const json& r) {
  std::vector<std::string> out{"orch " + id + ": " + r["status"].get<std::string>() + " — " + r["summary"].get<std::string>()};
  auto items = [&](const json& arr, auto fmt) {
    std::vector<std::string> v;
    for (const auto& x : arr) v.push_back(fmt(x));
    return join(v, "; ");
  };
  if (!r["files_changed"].empty())
    out.push_back("files: " + items(r["files_changed"], [](const json& f) { return f.value("path", "") + " (" + f.value("change", "") + ")"; }));
  if (!r["decisions"].empty()) out.push_back("decisions: " + items(r["decisions"], [](const json& d) { return d.get<std::string>(); }));
  if (!r["checks"].empty())
    out.push_back("checks: " + items(r["checks"], [](const json& c) { return c.value("command", "") + " → " + c.value("result", ""); }));
  if (!r["open_issues"].empty()) out.push_back("open: " + items(r["open_issues"], [](const json& o) { return o.get<std::string>(); }));
  out.push_back("next: " + r["next_action"].get<std::string>());
  return join(out, "\n");
}

std::string build_packet(const PacketInput& p) {
  std::string scope = p.mode == "write"
                          ? "You may edit files only inside `" + p.cwd + "` (a Git worktree on branch `" + p.branch + "`). Leave changes uncommitted for review."
                          : "Read-only: do not create, edit, or delete any file.";
  std::vector<std::string> lines = {"# Task", "", trim(p.task), "", "# Scope", "",
                                    "- Target: `" + p.target_rel + "` (working directory: `" + p.cwd + "`).",
                                    "- " + scope,
                                    "- Read the target's own AGENTS.md / CLAUDE.md rules if present and follow them.",
                                    "- Load only what the task needs: search first, then read file ranges.",
                                    "", "# Target state", ""};
  size_t budget = PACKET_FILE_BUDGET;
  bool any = false;
  for (const auto& f : STARTUP_FILES) {
    auto path = fs::path(p.target_abs) / f;
    if (!fs::exists(path) || budget == 0) continue;
    std::string raw = read_file(path);
    size_t cap = std::min(PER_FILE_CHARS, budget);
    std::string text = raw.size() > cap ? raw.substr(0, cap) : raw;
    budget -= text.size();
    any = true;
    lines.push_back("## " + f + (raw.size() > text.size() ? " (truncated)" : ""));
    lines.push_back("");
    lines.push_back(trim(text));
    lines.push_back("");
  }
  if (!any) {
    lines.push_back("(no startup files found)");
    lines.push_back("");
  }
  lines.insert(lines.end(), {"# Return", "",
                             "Finish with the structured result: status, summary (2-5 sentences), files_changed",
                             "(path + what changed), decisions (with reasons), checks you actually ran (pass/fail/not_run),",
                             "open_issues, and the exact next_action. Never report a check you did not run."});
  return join(lines, "\n");
}

std::string build_merge_message(const std::vector<MergeSource>& sources, const std::optional<std::string>& task) {
  std::vector<std::string> out = {"# Results merged from other sessions", "",
                                  "These results come from separate orchestrated sessions. Treat them as reports from colleagues:",
                                  "verify anything you rely on, and do not assume their files exist in your working directory.", ""};
  for (const auto& s : sources) {
    const json& r = s.result;
    out.insert(out.end(), {"## Session " + s.id + " — " + s.cli + ", " + s.mode + ", target `" + s.target + "`", "",
                           "Status: " + r["status"].get<std::string>(), "", "Summary: " + r["summary"].get<std::string>(), ""});
    auto list = [&](const std::string& title, const std::vector<std::string>& items) {
      if (items.empty()) return;
      out.push_back(title + ":");
      for (const auto& i : items) out.push_back("- " + i);
      out.push_back("");
    };
    std::vector<std::string> files, decisions, checks, open;
    for (const auto& f : r["files_changed"]) files.push_back(f.value("path", "") + ": " + f.value("change", ""));
    for (const auto& d : r["decisions"]) decisions.push_back(d.get<std::string>());
    for (const auto& c : r["checks"]) checks.push_back(c.value("command", "") + " → " + c.value("result", ""));
    for (const auto& o : r["open_issues"]) open.push_back(o.get<std::string>());
    list("Files changed", files);
    list("Decisions", decisions);
    list("Checks", checks);
    list("Open issues", open);
    out.push_back("Next action: " + r["next_action"].get<std::string>());
    out.push_back("");
    if (s.worktree && s.branch) {
      out.push_back("Code: changes live in worktree `" + *s.worktree + "` on branch `" + *s.branch +
                    "` (possibly uncommitted). Inspect them there; they are not in your working directory.");
      out.push_back("");
    }
  }
  out.insert(out.end(), {"# What to do", "",
                         trim(task.value_or("Integrate these results into your current work: say where they agree or conflict with your own findings, and what changes in your plan.")),
                         "", "Finish with the structured result again."});
  return join(out, "\n");
}

}  // namespace ws::orch
