#!/usr/bin/env python3
# Atomic patch #3: P2b-1 residual-maker self-inclusion knob.
import io, sys
path = "lib/infrastructure/scheduler/src/module_adapters.cpp"
s = io.open(path, encoding="utf-8").read()
orig = s
reps = []
reps.append((
"""static bool p2b_load_control_var(const std::string& out_dir,
                                 std::vector<P2bControlVar>* out,
                                 int* out_grid, std::string* why) {""",
"""static bool p2b_load_control_var(const std::string& out_dir,
                                 std::vector<P2bControlVar>* out,
                                 int* out_grid, bool include_self,
                                 std::string* why) {"""))
reps.append((
"""      ok = astrocs::v6::p2var::normalized_weight_hat_row(w.data(), n, k, false,
                                                         &row, &err) &&""",
"""      ok = astrocs::v6::p2var::normalized_weight_hat_row(w.data(), n, k,
                                                         include_self, &row,
                                                         &err) &&"""))
reps.append((
"""  const bool cvar_available =
      p2b_load_control_var(out_dir, &cvar_tab, &cvar_grid, &cvar_why);""",
"""  // variance_include_self: false（默认）= (c) 排除自身（与 P2a 重构口径一致）；
  // true = UPM 含自身的加权均值（W2 口径；此时"残差制造者 vs σ²+Var(ĝ)"
  // 差异显著——朴素式对 N 帧高估 (1+1/N)/(1-1/N)）。
  const bool cvar_include_self = doc.value("variance_include_self", false);
  const bool cvar_available = p2b_load_control_var(
      out_dir, &cvar_tab, &cvar_grid, cvar_include_self, &cvar_why);"""))
reps.append((
"""                       {"variance_model",
                        any_var_ok
                            ? "residual_maker_PSigmaPt_exclude_self_control"
                            : "unavailable"},""",
"""                       {"variance_model",
                        any_var_ok
                            ? (cvar_include_self
                                   ? "residual_maker_PSigmaPt_include_self_control"
                                   : "residual_maker_PSigmaPt_exclude_self_control")
                            : "unavailable"},"""))
for old, new in reps:
    n = s.count(old)
    if n != 1:
        sys.stderr.write("ANCHOR COUNT %d for: %r\n" % (n, old[:90]))
        sys.exit(2)
    s = s.replace(old, new)
io.open(path, "w", encoding="utf-8").write(s)
print("PATCH3_OK bytes %d -> %d" % (len(orig), len(s)))
