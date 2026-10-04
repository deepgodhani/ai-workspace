// `ws usage` and `ws session-report` — ports of bin/token-usage (bash + Python) and
// bin/session-report (Python). Both read local AI CLI logs through a pinned tokscale
// (`npx -y tokscale@<version> models --json …`). Read-only: never runs tokscale
// submit/autosubmit/login. Number formatting follows Python's format() rules, and sums
// follow Python 3.12+ `sum()` (Neumaier) and `statistics.mean` (exact), so the reports
// match the scripts digit for digit.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "commands.hpp"
#include "util/proc.hpp"

namespace ws {
namespace {

using json = nlohmann::json;

struct Fail {  // ends the command: message on stderr, exit `code`
  int code;
  std::string msg;
};
[[noreturn]] void fail(const std::string& msg, int code = 1) { throw Fail{code, msg}; }

// ---------------------------------------------------------------- Python numbers and format()

// A JSON number as Python sees it: int (exact) or float.
struct Num {
  bool is_float = false;
  long long i = 0;
  double f = 0;
  double d() const { return is_float ? f : static_cast<double>(i); }
};

Num num(const json& v, const std::string& what) {
  if (v.is_boolean()) return Num{false, v.get<bool>() ? 1 : 0, 0};
  if (v.is_number_integer()) return Num{false, v.get<long long>(), 0};
  if (v.is_number_float()) return Num{true, 0, v.get<double>()};
  fail("ws: tokscale output: " + what + " is not a number");
}

Num add(const Num& a, const Num& b) {
  if (!a.is_float && !b.is_float) return Num{false, a.i + b.i, 0};
  return Num{true, 0, a.d() + b.d()};
}

__extension__ typedef unsigned __int128 u128;

int bit_length(unsigned long long v) { return v ? 64 - __builtin_clzll(v) : 0; }

// a / b as Python computes it: int / int is rounded once from the exact quotient;
// otherwise both sides become floats first.
double py_div(const Num& a, const Num& b) {
  if (a.is_float || b.is_float || b.i == 0) return a.d() / b.d();
  const bool neg = (a.i < 0) != (b.i < 0);
  const unsigned long long ua = a.i < 0 ? 0ULL - static_cast<unsigned long long>(a.i) : static_cast<unsigned long long>(a.i);
  const unsigned long long ub = b.i < 0 ? 0ULL - static_cast<unsigned long long>(b.i) : static_cast<unsigned long long>(b.i);
  if (ua == 0) return neg ? -0.0 : 0.0;
  if (ua < (1ULL << 53) && ub < (1ULL << 53)) return a.d() / b.d();  // both exact as doubles
  // Scale so the quotient has >= 63 bits, then fold the remainder into a sticky bit.
  const int k = 64 + bit_length(ub) - bit_length(ua);  // 2..126; ua << k stays below 2^128
  const u128 num = static_cast<u128>(ua) << k;
  u128 q = num / ub;
  if (num % ub) q |= 1;
  const double r = std::ldexp(static_cast<double>(q), -k);
  return neg ? -r : r;
}

// repr(float): shortest round-trip digits; scientific when exp < -4 or exp >= 16.
std::string py_repr(double x) {
  if (std::isnan(x)) return "nan";
  if (std::isinf(x)) return x < 0 ? "-inf" : "inf";
  char buf[64];
  auto r = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::scientific);
  std::string s(buf, r.ptr);  // e.g. "-1.2345e+03"
  const size_t e = s.find('e');
  std::string mant = s.substr(0, e);
  const int exp = std::stoi(s.substr(e + 1));
  const bool neg = mant[0] == '-';
  if (neg) mant.erase(0, 1);
  std::string digits;
  for (char c : mant)
    if (c != '.') digits += c;
  std::string out;
  if (exp < -4 || exp >= 16) {
    out = digits.substr(0, 1);
    if (digits.size() > 1) out += "." + digits.substr(1);
    char eb[16];
    std::snprintf(eb, sizeof eb, "e%c%02d", exp < 0 ? '-' : '+', std::abs(exp));
    out += eb;
  } else if (exp < 0) {
    out = "0." + std::string(static_cast<size_t>(-exp - 1), '0') + digits;
  } else {
    const size_t ip = static_cast<size_t>(exp) + 1;
    if (digits.size() <= ip) out = digits + std::string(ip - digits.size(), '0') + ".0";
    else out = digits.substr(0, ip) + "." + digits.substr(ip);
  }
  return neg ? "-" + out : out;
}

// Insert ',' every three digits in the integer part of a plain decimal string.
std::string group(const std::string& s) {
  size_t start = (!s.empty() && s[0] == '-') ? 1 : 0;
  size_t end = start;
  while (end < s.size() && s[end] >= '0' && s[end] <= '9') ++end;
  if (s.find('e') != std::string::npos || end - start <= 3) return s;
  std::string ip = s.substr(start, end - start), g;
  for (size_t k = 0; k < ip.size(); ++k) {
    if (k && (ip.size() - k) % 3 == 0) g += ',';
    g += ip[k];
  }
  return s.substr(0, start) + g + s.substr(end);
}

// format(n, '' or ','): str(n), optionally grouped.
std::string py_num(const Num& n, bool comma = false) {
  const std::string s = n.is_float ? py_repr(n.f) : std::to_string(n.i);
  return comma ? group(s) : s;
}

// format(x, '.Nf') / format(x, ',.Nf').
std::string py_fixed(double x, int prec, bool comma = false) {
  if (std::isnan(x)) return "nan";
  if (std::isinf(x)) return x < 0 ? "-inf" : "inf";
  char buf[512];
  std::snprintf(buf, sizeof buf, "%.*f", prec, x);
  return comma ? group(buf) : buf;
}

size_t code_points(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}
std::string ljust(const std::string& s, size_t w) {
  const size_t n = code_points(s);
  return n >= w ? s : s + std::string(w - n, ' ');
}
std::string rjust(const std::string& s, size_t w) {
  const size_t n = code_points(s);
  return n >= w ? s : std::string(w - n, ' ') + s;
}
// s[:n] on code points.
std::string head(const std::string& s, size_t n) {
  size_t cps = 0, i = 0;
  for (; i < s.size(); ++i) {
    if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
      if (cps == n) break;
      ++cps;
    }
  }
  return s.substr(0, i);
}

// str(value) for a JSON value.
std::string py_str(const json& v) {
  if (v.is_string()) return v.get<std::string>();
  if (v.is_null()) return "None";
  if (v.is_boolean()) return v.get<bool>() ? "True" : "False";
  if (v.is_number()) return py_num(num(v, "value"));
  return v.dump();
}

bool truthy(const json& v) {
  if (v.is_null()) return false;
  if (v.is_boolean()) return v.get<bool>();
  if (v.is_number()) return v.get<double>() != 0;
  if (v.is_string()) return !v.get_ref<const std::string&>().empty();  // json::empty() is false for any string
  if (v.is_array() || v.is_object()) return !v.empty();
  return true;
}

const json& key(const json& obj, const char* k) {
  if (!obj.is_object() || !obj.contains(k)) fail(std::string("ws: tokscale output has no '") + k + "'");
  return obj.at(k);
}
json get(const json& obj, const char* k, json def) {
  if (!obj.is_object()) fail("ws: tokscale output entry is not an object");
  return obj.contains(k) ? obj.at(k) : def;
}

// Python 3.12+ built-in sum() over floats: Neumaier compensated summation.
double py_sum(const std::vector<double>& xs) {
  double f = 0, c = 0;
  for (double x : xs) {
    const double t = f + x;
    if (std::fabs(f) >= std::fabs(x)) c += (f - t) + x;
    else c += (x - t) + f;
    f = t;
  }
  if (c != 0 && std::isfinite(c)) f += c;
  return f;
}

// Correctly rounded sum (Shewchuk partials, as math.fsum).
double fsum(std::vector<double> xs) {
  std::vector<double> p;
  for (double x : xs) {
    size_t i = 0;
    for (double y : p) {
      if (std::fabs(x) < std::fabs(y)) std::swap(x, y);
      const double hi = x + y, lo = y - (hi - x);
      if (lo != 0) p[i++] = lo;
      x = hi;
    }
    p.resize(i);
    p.push_back(x);
  }
  double hi = 0;
  if (!p.empty()) {
    size_t n = p.size();
    hi = p[--n];
    double lo = 0;
    while (n > 0) {
      const double x = hi, y = p[--n];
      hi = x + y;
      lo = y - (hi - x);
      if (lo != 0) break;
    }
    if (n > 0 && ((lo < 0 && p[n - 1] < 0) || (lo > 0 && p[n - 1] > 0))) {
      const double y = lo * 2, x = hi + y;
      if (y == x - hi) hi = x;
    }
  }
  return hi;
}

// statistics.mean of floats: the exact mean, rounded once.
double py_mean(const std::vector<double>& xs) {
  const double n = static_cast<double>(xs.size());
  const double m0 = fsum(xs) / n;
  std::vector<double> rest = xs;
  const double p = n * m0;
  rest.push_back(-p);
  rest.push_back(-std::fma(n, m0, -p));
  return m0 + fsum(rest) / n;
}

double py_median(std::vector<double> xs) {  // statistics.median
  std::sort(xs.begin(), xs.end());
  const size_t n = xs.size();
  return n % 2 ? xs[n / 2] : (xs[n / 2 - 1] + xs[n / 2]) / 2;
}

// ---------------------------------------------------------------- tokscale

const char* TOKSCALE_PIN = "4.17.0";

json tokscale(const std::vector<std::string>& argv, bool hide_stderr) {
  if (!find_on_path("npx")) fail("npx not found (install Node.js)");
  RunResult r;
  try {
    if (hide_stderr) r = run(argv, RunOptions{});
    else r = run_capture_stdout(argv);
  } catch (const std::exception& e) {
    fail(std::string("ws: ") + e.what());
  }
  if (r.code != 0) {
    if (!hide_stderr) throw Fail{r.code > 0 ? r.code : 1, ""};  // npx already printed its error
    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "" : " ") + a;
    fail(r.err + "ws session-report: " + cmd + " failed (exit " + std::to_string(r.code) + ")");
  }
  try {
    return json::parse(r.out);
  } catch (const json::parse_error& e) {
    fail(std::string("ws: tokscale output is not valid JSON: ") + e.what());
  }
}

std::vector<std::string> tokscale_argv(const std::string& version, const std::string& clients, const std::string& group) {
  return {"npx", "-y", "tokscale@" + version, "models", "--json", "-c", clients, "--group-by", group};
}

// ---------------------------------------------------------------- ws usage

const char* USAGE_TEXT =
    "Usage: ws usage [--today|--week|--month|--since YYYY-MM-DD [--until YYYY-MM-DD]]\n"
    "                [--clients a,b] [--by-session] [--top N]\n"
    "\n"
    "Default: last 7 days, all four CLIs, grouped by client+model.\n"
    "--by-session   group per session (use for before/after comparison of one task)\n"
    "--top N        rows to print (default 15)\n";

// int(text) as Python parses it: surrounding whitespace, a sign, digits with single '_'.
std::optional<long long> py_int(const std::string& text) {
  size_t b = text.find_first_not_of(" \t\n\r\f\v"), e = text.find_last_not_of(" \t\n\r\f\v");
  if (b == std::string::npos) return std::nullopt;
  std::string s = text.substr(b, e - b + 1);
  bool neg = false;
  if (s[0] == '+' || s[0] == '-') neg = s[0] == '-', s.erase(0, 1);
  if (s.empty() || s.front() == '_' || s.back() == '_' || s.find("__") != std::string::npos) return std::nullopt;
  std::string digits;
  for (char c : s) {
    if (c == '_') continue;
    if (c < '0' || c > '9') return std::nullopt;
    digits += c;
  }
  try {
    long long v = std::stoll(digits);
    return neg ? -v : v;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

int usage_main(const std::vector<std::string>& args) {
  std::string version = TOKSCALE_PIN;
  if (const char* v = std::getenv("TOKSCALE_VERSION"); v && *v) version = v;
  std::string clients = "claude,codex,gemini,kiro", group = "client,model", top = "15";
  std::vector<std::string> period;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    const bool has_value = i + 1 < args.size();
    if (a == "--today" || a == "--week" || a == "--month") {
      period.push_back(a);
    } else if ((a == "--since" || a == "--until") && has_value) {
      period.push_back(a);
      period.push_back(args[++i]);
    } else if (a == "--clients" && has_value) {
      clients = args[++i];
    } else if (a == "--by-session") {
      group = "client,session,model";
    } else if (a == "--top" && has_value) {
      top = args[++i];
    } else if (a == "-h" || a == "--help") {
      std::cout << USAGE_TEXT;
      return 0;
    } else {
      std::cerr << USAGE_TEXT;
      return 2;
    }
  }
  if (period.empty()) period.push_back("--week");

  std::vector<std::string> argv = tokscale_argv(version, clients, group);
  argv.insert(argv.end(), period.begin(), period.end());
  const json d = tokscale(argv, false);
  const auto top_n = py_int(top);
  if (!top_n) fail("ws usage: --top: invalid integer '" + top + "'");

  std::vector<json> rows;
  const json entries = d.is_object() ? get(d, "entries", json::array()) : json();
  if (!d.is_object() || !entries.is_array()) fail("ws: tokscale output has no entry list");
  for (const auto& e : entries) rows.push_back(e);
  std::vector<std::pair<Num, size_t>> keyed;
  for (size_t i = 0; i < rows.size(); ++i) keyed.push_back({num(get(rows[i], "cost", 0), "cost"), i});
  std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first.d() > b.first.d(); });

  std::cout << ljust("client", 8) << " " << ljust("model", 22) << " " << ljust("session", 12) << " " << rjust("msgs", 6) << " "
            << rjust("tokens", 14) << " " << rjust("cache-read%", 11) << " " << rjust("out", 10) << " " << rjust("cost$", 9) << "\n";
  // rows[:top], including Python's negative slice bound.
  const long long n = static_cast<long long>(rows.size());
  const long long stop = *top_n >= 0 ? std::min(*top_n, n) : std::max(0LL, n + *top_n);
  for (long long k = 0; k < stop; ++k) {
    const json& e = rows[keyed[static_cast<size_t>(k)].second];
    const Num t = add(add(add(num(key(e, "input"), "input"), num(key(e, "output"), "output")), num(key(e, "cacheRead"), "cacheRead")),
                      num(key(e, "cacheWrite"), "cacheWrite"));
    const double cr = t.d() != 0 ? py_div(num(key(e, "cacheRead"), "cacheRead"), t) * 100 : 0;
    json sess = get(e, "sessionId", nullptr);
    if (!truthy(sess)) sess = get(e, "session", nullptr);
    const std::string session = truthy(sess) ? head(py_str(sess), 12) : "";
    std::cout << ljust(py_str(key(e, "client")), 8) << " " << ljust(head(py_str(key(e, "model")), 22), 22) << " " << ljust(session, 12)
              << " " << rjust(py_num(num(get(e, "messageCount", 0), "messageCount")), 6) << " " << rjust(py_num(t, true), 14) << " "
              << rjust(py_fixed(cr, 1), 10) << "% " << rjust(py_num(num(key(e, "output"), "output"), true), 10) << " "
              << rjust(py_fixed(num(get(e, "cost", 0), "cost").d(), 2), 9) << "\n";
  }
  const Num tin = num(key(d, "totalInput"), "totalInput"), tout = num(key(d, "totalOutput"), "totalOutput");
  const Num tcr = num(key(d, "totalCacheRead"), "totalCacheRead"), tcw = num(key(d, "totalCacheWrite"), "totalCacheWrite");
  const Num T = add(add(add(tin, tout), tcr), tcw);
  const double crp = T.d() != 0 ? py_div(tcr, T) * 100 : 0;
  std::cout << "\nTOTAL  rows=" << rows.size() << "  messages=" << py_num(num(get(d, "totalMessages", 0), "totalMessages"), true)
            << "  tokens=" << py_num(T, true) << "  cache-read=" << py_fixed(crp, 1) << "%  output=" << py_num(tout, true) << "  cost=$"
            << py_fixed(num(get(d, "totalCost", 0), "totalCost").d(), 2) << "\n";
  std::cout << "cost = API-list-price estimate from local logs; subscription/credit billing differs.\n";
  return 0;
}

// ---------------------------------------------------------------- ws session-report

const char* SR_USAGE_LINE = "usage: ws session-report [-h] [--since SINCE] [--clients CLIENTS]\n";
const char* SR_HELP =
    "usage: ws session-report [-h] [--since SINCE] [--clients CLIENTS]\n"
    "\n"
    "options:\n"
    "  -h, --help         show this help message and exit\n"
    "  --since SINCE\n"
    "  --clients CLIENTS\n";

[[noreturn]] void sr_usage_error(const std::string& msg) { fail(std::string(SR_USAGE_LINE) + "ws session-report: error: " + msg, 2); }

// argparse treats "-5" or "-.5" as a value (the parser defines no negative-number-like options).
bool looks_negative_number(const std::string& s) {
  if (s.size() < 2 || s[0] != '-') return false;
  bool digit = false, dot = false;
  for (size_t i = 1; i < s.size(); ++i) {
    if (s[i] >= '0' && s[i] <= '9') digit = true;
    else if (s[i] == '.' && !dot) dot = true;
    else return false;
  }
  return digit;
}

int session_report_main(const std::vector<std::string>& args) {
  std::optional<std::string> since;
  std::string clients = "claude";
  std::vector<std::string> extras;
  bool only_positional = false;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& t = args[i];
    if (only_positional || t.empty() || t[0] != '-' || t == "-" || looks_negative_number(t)) {
      extras.push_back(t);
      continue;
    }
    if (t == "--") {
      only_positional = true;
      continue;
    }
    if (t == "-h") {
      std::cout << SR_HELP;
      return 0;
    }
    std::string full;
    const size_t eq = t.find('=');
    const std::string name = t.substr(0, eq);
    if (name.size() > 2 && name[1] == '-')
      for (const char* opt : {"--help", "--since", "--clients"})  // unique prefixes are accepted
        if (std::string(opt).compare(0, name.size(), name) == 0) full = opt;
    if (full == "--help") {
      if (eq != std::string::npos) sr_usage_error("argument -h/--help: ignored explicit argument '" + t.substr(eq + 1) + "'");
      std::cout << SR_HELP;
      return 0;
    }
    if (full.empty()) {
      extras.push_back(t);
      continue;
    }
    std::string value;
    if (eq != std::string::npos) {
      value = t.substr(eq + 1);
    } else if (i + 1 < args.size() && (args[i + 1].empty() || args[i + 1][0] != '-' || args[i + 1] == "-" || looks_negative_number(args[i + 1]))) {
      value = args[++i];
    } else {
      sr_usage_error("argument " + full + ": expected one argument");
    }
    if (full == "--since") since = value;
    else clients = value;
  }
  if (!extras.empty()) {
    std::string s;
    for (const auto& e : extras) s += (s.empty() ? "" : " ") + e;
    sr_usage_error("unrecognized arguments: " + s);
  }

  std::vector<std::string> argv = tokscale_argv(TOKSCALE_PIN, clients, "client,session,model");
  if (since && !since->empty()) argv.insert(argv.end(), {"--since", *since});
  else argv.push_back("--month");
  const json d = tokscale(argv, true);
  const json& entries = key(d, "entries");
  if (!entries.is_array()) fail("ws: tokscale output has no entry list");

  struct Session {
    Num msgs, tokens;
    Num cost;  // Python: 0.0 += cost, so always a float
  };
  std::vector<std::string> order;  // dict insertion order
  std::map<std::string, Session> S;
  for (const auto& x : entries) {
    const std::string k = get(x, "sessionId", nullptr).dump();
    if (!S.count(k)) {
      order.push_back(k);
      S[k] = Session{Num{}, Num{}, Num{true, 0, 0.0}};
    }
    Session& s = S[k];
    s.msgs = add(s.msgs, num(get(x, "messageCount", 0), "messageCount"));
    s.tokens = add(s.tokens, add(add(add(num(key(x, "input"), "input"), num(key(x, "output"), "output")), num(key(x, "cacheRead"), "cacheRead")),
                                 num(key(x, "cacheWrite"), "cacheWrite")));
    s.cost = add(s.cost, num(get(x, "cost", 0), "cost"));
  }
  std::vector<Session> ss;
  for (const auto& k : order)
    if (S[k].msgs.d() > 0) ss.push_back(S[k]);
  if (ss.size() < 3) fail("not enough sessions in range");

  std::vector<double> costs;
  for (const auto& v : ss) costs.push_back(v.cost.d());
  double total = py_sum(costs);
  if (total == 0) total = 1;
  std::cout << ss.size() << " sessions, $" << py_fixed(total, 0, true) << " estimated (API list price)\n";
  std::cout << rjust("msgs/session", 13) << " " << rjust("sessions", 8) << " " << rjust("median tok/msg", 15) << " " << rjust("cost share", 10)
            << "\n";
  const std::pair<long long, long long> buckets[] = {{1, 20}, {21, 50}, {51, 100}, {101, 200}, {201, 400}, {401, 800}, {801, 1000000000}};
  for (const auto& [lo, hi] : buckets) {
    std::vector<double> per_msg, cost;
    for (const auto& v : ss) {
      const double m = v.msgs.d();
      if (static_cast<double>(lo) <= m && m <= static_cast<double>(hi)) {
        per_msg.push_back(py_div(v.tokens, v.msgs));
        cost.push_back(v.cost.d());
      }
    }
    if (per_msg.empty()) continue;
    const std::string lab = hi < 1000000000 ? std::to_string(lo) + "-" + std::to_string(hi) : std::to_string(lo) + "+";
    const double med = std::trunc(py_median(per_msg));  // int(): toward zero
    std::cout << rjust(lab, 13) << " " << rjust(std::to_string(per_msg.size()), 8) << " " << rjust(py_fixed(med, 0, true), 15) << " "
              << rjust(py_fixed(py_sum(cost) / total * 100, 1), 9) << "%\n";
  }
  std::vector<double> lx, ys;
  for (const auto& v : ss) {
    lx.push_back(std::log(v.msgs.d()));
    ys.push_back(py_div(v.tokens, v.msgs));
  }
  const double mx = py_mean(lx), my = py_mean(ys);
  std::vector<double> sx, sy, sxy;
  for (size_t i = 0; i < lx.size(); ++i) {
    sx.push_back((lx[i] - mx) * (lx[i] - mx));
    sy.push_back((ys[i] - my) * (ys[i] - my));
    sxy.push_back((lx[i] - mx) * (ys[i] - my));
  }
  double den = std::sqrt(py_sum(sx) * py_sum(sy));
  if (den == 0) den = 1;
  std::cout << "pearson r(log messages, tokens per message) = " << py_fixed(py_sum(sxy) / den, 2) << "\n";
  return 0;
}

int guarded(int (*fn)(const std::vector<std::string>&), const std::vector<std::string>& args) {
  try {
    return fn(args);
  } catch (const Fail& f) {
    std::cout.flush();
    if (!f.msg.empty()) std::cerr << f.msg << "\n";
    return f.code;
  }
}

}  // namespace

int cmd_usage(const std::vector<std::string>& args) { return guarded(usage_main, args); }
int cmd_session_report(const std::vector<std::string>& args) { return guarded(session_report_main, args); }

}  // namespace ws
