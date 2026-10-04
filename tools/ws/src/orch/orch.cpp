#include <algorithm>
#include <cstdio>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "commands.hpp"
#include "orch/acp.hpp"
#include "orch/adapters.hpp"
#include "orch/packet.hpp"
#include "orch/registry.hpp"
#include "orch/sandbox.hpp"
#include "orch/worktree.hpp"
#include "util/args.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace ws {
using namespace ws::orch;
using json = nlohmann::json;

namespace {

const char* USAGE = R"(orch — start scoped AI CLI child sessions with a compact context packet

  orch spawn <target> --task "…" [--cli claude|codex|kiro] [--write] [--allow TOOL]…
                      [--model M] [--budget USD] [--timeout SEC] [--parent ID] [--full]
                      [--sandbox auto|none|native|srt] [--dry-run] [--json]
  orch resume <id> --task "…" [--budget USD] [--timeout SEC] [--json]
  orch merge <src-id>… --into <id> [--task "…"] [--budget USD] [--timeout SEC] [--json]
  orch list [--all]
  orch show <id>
  orch diff <id>
  orch close <id> [--force] [--delete-branch]

--task - reads the task from stdin. Default --cli claude, read-only, --timeout 1800.
Claude children start lean (no skills, no MCP servers, cache-friendly prompt); --full keeps them.
--sandbox auto (default): Claude/Codex use their native sandbox, Kiro runs inside srt.)";

const std::vector<OptSpec> SPEC = {
    {"task", true}, {"cli", true, false, "claude"}, {"write"}, {"full"}, {"sandbox", true, false, "auto"}, {"allow", true, true},
    {"model", true}, {"budget", true}, {"timeout", true, false, "1800"}, {"parent", true}, {"into", true}, {"dry-run"}, {"json"},
    {"all"}, {"force"}, {"delete-branch"}, {"help"},
    // hidden, for tests
    {"mode", true}, {"cwd", true}, {"kind", true}, {"path", true, true}, {"command", true},
};

std::string fixed(double v, int places) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*f", places, v);
  return buf;
}

std::string pad(const std::string& s, size_t n) { return s.size() >= n ? s : s + std::string(n - s.size(), ' '); }

std::string tail(const std::string& s, size_t n) {
  std::string t = trim(s);
  return t.size() > n ? t.substr(t.size() - n) : t;
}

std::string text_of(const Row& r, const std::string& col, const std::string& fallback = "") { return as_text(r, col).value_or(fallback); }

std::string read_task(const Args& a) {
  auto t = a.get("task");
  if (!t) throw std::runtime_error("--task is required (use --task - to read stdin)");
  std::string task = *t == "-" ? read_stdin() : *t;
  if (trim(task).empty()) throw std::runtime_error("task is empty");
  return task;
}

std::string cli_of(const Args& a) {
  std::string c = a.get_or("cli", "claude");
  if (c != "claude" && c != "codex" && c != "kiro") throw std::runtime_error("--cli must be claude, codex or kiro, got " + c);
  return c;
}

std::string run_dir(const fs::path& ws, const std::string& id) {
  fs::path d = ws / ".orch" / "runs" / id;
  fs::create_directories(d);
  return d.string();
}

struct Target {
  std::string abs, rel;
};

Target resolve_target(const fs::path& ws, const std::string& input) {
  fs::path p = fs::path(input).is_absolute() ? fs::path(input) : ws / input;
  if (!fs::exists(p)) throw std::runtime_error("no such directory: " + input);
  fs::path abs = fs::canonical(p);
  if (!fs::is_directory(abs)) throw std::runtime_error("target is not a directory: " + input);
  fs::path rel = abs.lexically_relative(ws);
  if (rel.empty() || rel == "." || *rel.begin() == "..")
    throw std::runtime_error("target must be a folder inside the workspace (" + ws.string() + "), not the root or outside it: " + input);
  return {abs.string(), rel.string()};
}

std::string git_snapshot(const std::string& root) {
  return run({"git", "status", "--porcelain", "--untracked-files=all"}, RunOptions{root, "", 0, {}}).out;
}

std::vector<std::string> writable_paths(const std::string& mode, const std::optional<std::string>& worktree) {
  if (mode != "write" || !worktree) return {};
  std::string git_dir = trim(run({"git", "rev-parse", "--absolute-git-dir"}, RunOptions{*worktree, "", 0, {}}).out);
  if (git_dir.empty()) return {*worktree};
  return {*worktree, git_dir};
}

std::string cost_of(const Row& r) {
  if (auto c = as_real(r, "cost_usd")) return "$" + fixed(*c, 3);
  if (auto cr = as_real(r, "credits"); cr && *cr != 0) return fixed(*cr, 2) + "cr";
  return "-";
}

json row_json(const Row& r) {
  json j = json::object();
  for (const auto& [k, v] : r) {
    if (k == "result_json") continue;
    if (std::holds_alternative<std::monostate>(v)) j[k] = nullptr;
    else if (auto i = std::get_if<long long>(&v)) j[k] = *i;
    else if (auto d = std::get_if<double>(&v)) j[k] = *d;
    else j[k] = std::get<std::string>(v);
  }
  auto rj = as_text(r, "result_json");
  j["result"] = rj ? json::parse(*rj) : json(nullptr);
  return j;
}

std::string footer(const Row& r) {
  std::string cli = text_of(r, "cli");
  std::vector<std::string> parts = {cli + " session " + text_of(r, "cli_session_id", "?")};
  if (cli != "kiro")
    parts.push_back("tokens in " + std::to_string(as_int(r, "input_tokens")) + " / cache-read " + std::to_string(as_int(r, "cache_read_tokens")) +
                    " / out " + std::to_string(as_int(r, "output_tokens")));
  std::string c = cost_of(r);
  parts.push_back("cost " + (c == "-" ? std::string("n/a") : c));
  parts.push_back("sandbox " + text_of(r, "sandbox", "none"));
  if (auto wt = as_text(r, "worktree")) parts.push_back("worktree " + *wt + " (" + text_of(r, "branch") + ")");
  std::string out;
  for (size_t i = 0; i < parts.size(); ++i) out += (i ? " | " : "") + parts[i];
  return out;
}

json opt_json(const std::optional<std::string>& s) { return s ? json(*s) : json(nullptr); }
json opt_json(const std::optional<double>& d) { return d ? json(*d) : json(nullptr); }

int execute(const fs::path& ws, Registry& reg, const std::string& id, const std::string& cli, const SpawnOpts& opts, int timeout_sec, bool as_json) {
  std::string dir = run_dir(ws, id);
  write_file(fs::path(dir) / "prompt.md", opts.prompt);
  std::optional<std::string> root = opts.mode == "read" ? repo_root(opts.cwd) : std::nullopt;
  std::optional<std::string> before = root ? std::optional<std::string>(git_snapshot(*root)) : std::nullopt;
  Row row0 = *reg.get(id);
  std::vector<std::string> wrapper;
  if (opts.sandbox == "srt") wrapper = srt_wrapper(cli, writable_paths(opts.mode, as_text(row0, "worktree")), dir);

  ChildOutput out;
  std::optional<long long> code;
  bool timed_out = false;
  std::vector<std::string> denied;
  if (cli == "kiro") {
    AcpRun ar;
    ar.cwd = opts.cwd;
    ar.mode = opts.mode;
    ar.prompt = opts.prompt;
    ar.resume_id = opts.resume_id;
    ar.model = opts.model;
    ar.allow = opts.allow;
    ar.timeout_ms = timeout_sec * 1000;
    ar.log_file = (fs::path(dir) / "acp.jsonl").string();
    ar.wrapper = wrapper;
    AcpOutput k = run_acp(ar);
    out = k;
    timed_out = k.timed_out;
    denied = k.denied;
    code = (k.error && !k.result) ? 1 : 0;
    if (!denied.empty()) {
      std::string d;
      for (const auto& x : denied) d += x + "\n";
      write_file(fs::path(dir) / "denied.txt", d);
    }
  } else {
    Invocation inv = cli == "claude" ? claude_invocation(opts) : codex_invocation(opts);
    std::vector<std::string> argv = wrapper;
    argv.insert(argv.end(), inv.argv.begin(), inv.argv.end());
    RunResult r;
    try {
      r = run(argv, RunOptions{opts.cwd, inv.input, timeout_sec * 1000, {}});
    } catch (const std::exception& e) {
      r.code = 127;
      r.err = e.what();
    }
    write_file(fs::path(dir) / "stdout.txt", r.out);
    write_file(fs::path(dir) / "stderr.txt", r.err);
    std::string last = fs::exists(opts.last_message_file) ? read_file(opts.last_message_file) : "";
    out = cli == "claude" ? parse_claude(r.out) : parse_codex(r.out, last);
    code = r.code;
    timed_out = r.timed_out;
    if (r.timed_out) out.error = "timed out after " + std::to_string(timeout_sec) + "s";
    else if (r.code != 0 && !out.error) out.error = argv[0] + " exited " + std::to_string(r.code) + ": " + tail(r.err, 300);
  }

  std::string status = out.result ? (*out.result)["status"].get<std::string>() : (timed_out ? "timeout" : "error");
  if (root && before) {
    std::string after = git_snapshot(*root);
    if (after != *before) {
      status = "violation";
      out.error = "read-only session changed files in " + *root + ":\n" + after;
    }
  }
  Finish f;
  f.status = status;
  f.exit_code = code;
  f.cli_session_id = out.cli_session_id;
  f.cost_usd = out.cost_usd;
  f.credits = out.credits != 0 ? std::optional<double>(out.credits) : std::nullopt;
  f.input_tokens = out.input_tokens;
  f.output_tokens = out.output_tokens;
  f.cache_read_tokens = out.cache_read_tokens;
  if (out.result) f.result_json = out.result->dump();
  f.error = out.error;
  reg.finish(id, f);
  Row row = *reg.get(id);

  if (as_json) {
    json j = {{"id", id}, {"status", status}, {"result", out.result ? *out.result : json(nullptr)}, {"error", opt_json(out.error)}, {"denied", denied},
              {"usage", {{"input", as_int(row, "input_tokens")}, {"cache_read", as_int(row, "cache_read_tokens")}, {"output", as_int(row, "output_tokens")},
                         {"cost_usd", opt_json(as_real(row, "cost_usd"))}, {"credits", opt_json(as_real(row, "credits"))}}},
              {"worktree", opt_json(as_text(row, "worktree"))}, {"branch", opt_json(as_text(row, "branch"))}};
    std::cout << j.dump(2) << "\n";
  } else {
    if (status == "violation") {
      std::cout << "orch " << id << ": violation — " << *out.error << "\n";
      if (out.result) std::cout << "child reported (untrusted): " << render_result(id, *out.result) << "\n";
    } else {
      std::cout << (out.result ? render_result(id, *out.result) : "orch " + id + ": " + status + " — " + out.error.value_or("")) << "\n";
      if (out.result && out.error) std::cout << "warning: " << *out.error << "\n";
    }
    if (!denied.empty()) {
      std::string first;
      for (size_t i = 0; i < denied.size() && i < 3; ++i) first += (i ? "; " : "") + denied[i];
      std::cout << "denied " << denied.size() << " tool request(s): " << first << (denied.size() > 3 ? "; …" : "") << "\n";
    }
    std::cout << footer(row) << "\n";
  }
  return status == "done" || status == "partial" ? 0 : 1;
}

std::optional<double> budget_of(const Args& a) {
  if (auto b = a.get("budget")) return std::stod(*b);
  return std::nullopt;
}

int spawn_cmd(const Args& a) {
  fs::path ws = find_workspace();
  if (a.positionals().empty()) throw std::runtime_error("spawn needs a <target>");
  Target target = resolve_target(ws, a.positionals()[0]);
  std::string cli = cli_of(a);
  std::string sandbox = resolve_tier(a.get_or("sandbox", "auto"), cli);
  std::string task = read_task(a);
  std::string mode = a.flag("write") ? "write" : "read";
  std::string id = random_hex(8);
  Registry reg(ws.string());
  if (auto p = a.get("parent"); p && !reg.get(*p)) throw std::runtime_error("unknown --parent " + *p);

  if (a.flag("dry-run")) {
    std::string prompt = build_packet({task, target.rel, target.abs, target.abs, mode, mode == "write" ? "orch/" + id : ""});
    std::cout << "dry run — nothing started" << (mode == "write" ? "; a worktree would be created" : "") << "\n";
    std::cout << "sandbox: " << sandbox << (sandbox == "srt" ? " (srt profile written per run)" : "") << "\n";
    if (cli == "kiro") {
      auto k = acp_command(a.get("model"));
      std::string cmd;
      for (const auto& x : k) cmd += (cmd.empty() ? "" : " ") + x;
      std::string full = prompt + json_instruction();
      std::cout << "command: " << cmd << "  (Agent Client Protocol over stdio; orch answers permission requests)\n";
      std::cout << "packet: " << full.size() << " chars (~" << (full.size() + 2) / 4 << " tokens)\n\n" << full << "\n";
      return 0;
    }
    SpawnOpts o;
    o.mode = mode;
    o.prompt = prompt;
    o.cwd = target.abs;
    o.session_id = uuid_v4();
    o.model = a.get("model");
    o.budget_usd = budget_of(a);
    o.allow = a.all("allow");
    o.full = a.flag("full");
    o.sandbox = sandbox;
    o.schema_file = "<run>/schema.json";
    o.last_message_file = "<run>/last.txt";
    Invocation inv = cli == "claude" ? claude_invocation(o) : codex_invocation(o);
    std::string shown;
    for (size_t i = 1; i < inv.argv.size(); ++i) {
      const auto& x = inv.argv[i];
      shown += " " + (x.size() > 60 ? "'" + x.substr(0, 40) + "…'" : x);
    }
    std::cout << "command: " << inv.argv[0] << shown << "\n";
    std::cout << "packet: " << prompt.size() << " chars (~" << (prompt.size() + 2) / 4 << " tokens)\n\n" << prompt << "\n";
    return 0;
  }

  std::optional<Worktree> wt;
  if (mode == "write") wt = create_worktree(ws.string(), target.abs, id);
  std::string cwd = wt ? wt->cwd : target.abs;
  std::string prompt = build_packet({task, target.rel, target.abs, cwd, mode, wt ? wt->branch : ""});
  std::string dir = run_dir(ws, id);
  std::string schema_file = (fs::path(dir) / "schema.json").string();
  write_file(schema_file, result_schema().dump());
  std::optional<std::string> session_id = cli == "claude" ? std::optional<std::string>(uuid_v4()) : std::nullopt;
  auto allow = a.all("allow");

  NewSession s;
  s.id = id;
  s.cli = cli;
  s.cli_session_id = session_id;
  s.parent_id = a.get("parent");
  s.target = target.rel;
  s.cwd = cwd;
  s.mode = mode;
  s.task = task;
  if (wt) {
    s.worktree = wt->path;
    s.branch = wt->branch;
    s.base = wt->base;
  }
  if (!allow.empty()) s.allow_json = json(allow).dump();
  s.full = a.flag("full") ? 1 : 0;
  s.model = a.get("model");
  s.sandbox = sandbox;
  reg.insert(s);

  SpawnOpts o;
  o.mode = mode;
  o.prompt = prompt;
  o.cwd = cwd;
  o.session_id = session_id;
  o.model = a.get("model");
  o.budget_usd = budget_of(a);
  o.allow = allow;
  o.full = a.flag("full");
  o.sandbox = sandbox;
  o.schema_file = schema_file;
  o.last_message_file = (fs::path(dir) / "last.txt").string();
  return execute(ws, reg, id, cli, o, std::stoi(a.get_or("timeout", "1800")), a.flag("json"));
}

Row resumable(Registry& reg, const std::optional<std::string>& id_arg, const std::string& role) {
  auto row = id_arg ? reg.get(*id_arg) : std::nullopt;
  if (!row) throw std::runtime_error("unknown " + role + (role.empty() ? "" : " ") + "session " + id_arg.value_or(""));
  std::string id = text_of(*row, "id");
  if (!as_text(*row, "cli_session_id")) throw std::runtime_error("session " + id + " has no CLI session id to resume");
  if (as_int(*row, "closed")) throw std::runtime_error("session " + id + " is closed");
  if (text_of(*row, "status") == "running") throw std::runtime_error("session " + id + " is still running; wait for it to finish before resuming");
  return *row;
}

int continue_session(const fs::path& ws, Registry& reg, const Row& prev, const std::string& prompt, const std::string& task, const Args& a,
                     const std::optional<std::string>& merged_from) {
  std::string id = random_hex(8);
  std::string dir = run_dir(ws, id);
  std::string schema_file = (fs::path(dir) / "schema.json").string();
  write_file(schema_file, result_schema().dump());
  std::vector<std::string> allow;
  if (auto aj = as_text(prev, "allow_json")) allow = json::parse(*aj).get<std::vector<std::string>>();

  NewSession s;
  s.id = id;
  s.cli = text_of(prev, "cli");
  s.cli_session_id = as_text(prev, "cli_session_id");
  s.parent_id = text_of(prev, "id");
  s.target = text_of(prev, "target");
  s.cwd = text_of(prev, "cwd");
  s.mode = text_of(prev, "mode");
  s.worktree = as_text(prev, "worktree");
  s.branch = as_text(prev, "branch");
  s.base = as_text(prev, "base");
  s.task = task;
  s.merged_from = merged_from;
  s.allow_json = as_text(prev, "allow_json");
  s.full = as_int(prev, "full");
  s.model = as_text(prev, "model");
  s.sandbox = as_text(prev, "sandbox");
  reg.insert(s);

  SpawnOpts o;
  o.mode = s.mode;
  o.prompt = prompt;
  o.cwd = s.cwd;
  o.resume_id = s.cli_session_id;
  o.allow = allow;
  o.full = s.full == 1;
  o.model = s.model;
  o.budget_usd = budget_of(a);
  o.sandbox = s.sandbox.value_or("none");
  o.schema_file = schema_file;
  o.last_message_file = (fs::path(dir) / "last.txt").string();
  return execute(ws, reg, id, s.cli, o, std::stoi(a.get_or("timeout", "1800")), a.flag("json"));
}

int resume_cmd(const Args& a) {
  fs::path ws = find_workspace();
  Registry reg(ws.string());
  Row prev = resumable(reg, a.positionals().empty() ? std::nullopt : std::optional<std::string>(a.positionals()[0]), "");
  std::string task = read_task(a);
  std::string prompt = trim(task) + "\n\nSame scope and rules as before. Finish with the structured result again.";
  return continue_session(ws, reg, prev, prompt, task, a, std::nullopt);
}

int merge_cmd(const Args& a) {
  fs::path ws = find_workspace();
  Registry reg(ws.string());
  if (!a.get("into")) throw std::runtime_error("merge needs --into <session id>");
  if (a.positionals().empty()) throw std::runtime_error("merge needs at least one source session id");
  Row dest = resumable(reg, a.get("into"), "destination");
  std::vector<MergeSource> sources;
  std::vector<std::string> ids;
  for (const auto& arg : a.positionals()) {
    auto src = reg.get(arg);
    if (!src) throw std::runtime_error("unknown source session " + arg);
    std::string sid = text_of(*src, "id");
    bool same_session = as_text(*src, "cli_session_id") && as_text(*src, "cli_session_id") == as_text(dest, "cli_session_id") &&
                        text_of(*src, "cli") == text_of(dest, "cli");
    if (sid == text_of(dest, "id") || same_session) throw std::runtime_error("source " + sid + " is the destination session itself");
    auto rj = as_text(*src, "result_json");
    if (!rj) throw std::runtime_error("source " + sid + " has no result to merge (status " + text_of(*src, "status") + ")");
    if (std::find(ids.begin(), ids.end(), sid) != ids.end()) continue;
    ids.push_back(sid);
    sources.push_back({sid, text_of(*src, "cli"), text_of(*src, "mode"), text_of(*src, "target"), json::parse(*rj), as_text(*src, "worktree"),
                       as_text(*src, "branch")});
  }
  std::optional<std::string> task = a.get("task") ? std::optional<std::string>(read_task(a)) : std::nullopt;
  std::string prompt = build_merge_message(sources, task);
  std::string joined;
  for (const auto& i : ids) joined += (joined.empty() ? "" : ",") + i;
  std::string label = "merge " + joined + (task ? ": " + trim(*task) : "");
  return continue_session(ws, reg, dest, prompt, label, a, joined);
}

int list_cmd(const Args& a) {
  Registry reg(find_workspace().string());
  auto rows = reg.list(a.flag("all"));
  if (rows.empty()) {
    std::cout << "no sessions\n";
    return 0;
  }
  std::cout << pad("id", 9) << pad("cli", 7) << pad("mode", 6) << pad("status", 9) << pad("cost", 9) << pad("parent", 9) << pad("merged", 18) << "target\n";
  for (const auto& r : rows) {
    std::string status = text_of(r, "status") + (as_int(r, "closed") ? "*" : "");
    std::string merged = text_of(r, "merged_from", "-").substr(0, 17);
    std::cout << pad(text_of(r, "id"), 9) << pad(text_of(r, "cli"), 7) << pad(text_of(r, "mode"), 6) << pad(status, 9) << pad(cost_of(r), 9)
              << pad(text_of(r, "parent_id", "-"), 9) << pad(merged, 18) << text_of(r, "target") << "\n";
  }
  if (a.flag("all")) std::cout << "* closed\n";
  return 0;
}

Row need_row(Registry& reg, const Args& a) {
  auto row = a.positionals().empty() ? std::nullopt : reg.get(a.positionals()[0]);
  if (!row) throw std::runtime_error("unknown session " + (a.positionals().empty() ? std::string() : a.positionals()[0]));
  return *row;
}

}  // namespace

int cmd_orch(const std::vector<std::string>& argv) {
  if (argv.empty() || argv[0] == "-h" || argv[0] == "--help" || argv[0] == "help") {
    std::cout << USAGE << "\n";
    return 0;
  }
  std::string cmd = argv[0];
  Args a(std::vector<std::string>(argv.begin() + 1, argv.end()), SPEC);
  if (a.flag("help")) {
    std::cout << USAGE << "\n";
    return 0;
  }
  if (cmd == "spawn") return spawn_cmd(a);
  if (cmd == "resume") return resume_cmd(a);
  if (cmd == "merge") return merge_cmd(a);
  if (cmd == "list") return list_cmd(a);
  if (cmd == "show") {
    Registry reg(find_workspace().string());
    std::cout << row_json(need_row(reg, a)).dump(2) << "\n";
    return 0;
  }
  if (cmd == "diff") {
    Registry reg(find_workspace().string());
    Row row = need_row(reg, a);
    auto wt = as_text(row, "worktree");
    auto base = as_text(row, "base");
    if (!wt || !base) throw std::runtime_error("session " + text_of(row, "id") + " is read-only; no worktree");
    std::cout << worktree_status(*wt, *base) << "\n";
    return 0;
  }
  if (cmd == "close") {
    Registry reg(find_workspace().string());
    Row row = need_row(reg, a);
    auto wt = as_text(row, "worktree");
    auto branch = as_text(row, "branch");
    if (wt && branch) remove_worktree(*wt, *branch, a.flag("force"), a.flag("delete-branch"));
    reg.close(text_of(row, "id"));
    std::cout << "closed " << text_of(row, "id") << (wt ? " and removed " + *wt : "")
              << (a.flag("delete-branch") && branch ? "; deleted " + *branch : "") << "\n";
    return 0;
  }
  if (cmd == "_decide") {
    PermissionRequest req;
    req.kind = a.get("kind");
    req.command = a.get("command");
    req.paths = a.all("path");
    if (auto c = a.get("command"); c && a.positionals().size() > 0) req.title = a.positionals()[0];
    Decision d = decide(req, a.get_or("mode", "read"), a.get_or("cwd", "/"), a.all("allow"));
    std::cout << (d.allow ? "allow" : "deny") << " " << d.reason << "\n";
    return 0;
  }
  if (cmd == "_parse") {
    auto r = parse_result(read_stdin());
    std::cout << (r ? r->dump() : "null") << "\n";
    return 0;
  }
  throw std::runtime_error("unknown command '" + cmd + "'\n\n" + USAGE);
}

}  // namespace ws
