#!/usr/bin/env python3
"""IMPL-P3-RSMP-001 负向 mutation 驱动器。

对 lib/algorithms/resample 的影子副本注入违反冻结条款的实现，重新编译并运行本任务
共址测试；断言至少一个测试变红（rc!=0）。原始树只读，不修改。

证据：run/v6/p3-rsmp/mutants/<id>/、run/v6/p3-rsmp/logs/、mutation_summary.json
"""
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))

LIB = os.path.join(ROOT, "lib", "phase3_rsmp")
WORK = os.path.join(ROOT, "run", "v6", "p3-rsmp")
MUT = os.path.join(WORK, "mutants")
LOGS = os.path.join(WORK, "logs")
BIN = os.path.join(WORK, "mutants_bin")
TESTS = ["p3_rsmp_core_test", "p3_rsmp_oracle_test", "p3_rsmp_gate_test"]
CXXFLAGS = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion"]

# (id, frozen_node, [(relative_file, old, new), ...])
MUTATIONS = [
    ("m_row_unnormalised", "FZ-P3-KERNEL-REGISTRY / R 行归一",
     [("p3_rsmp_operator.cpp",
       "      w = en.overlap_sr / oo;",
       "      w = en.overlap_sr;  // MUTATION: row not normalised")]),
    ("m_col_unnormalised", "SPEC 1.3 S 列归一 / 通量守恒",
     [("p3_rsmp_operator.cpp",
       "      w = en.overlap_sr / oi;",
       "      w = en.overlap_sr;  // MUTATION: column not normalised")]),
    ("m_diagonal_shortcut_cov", "FZ-FORMULA-COV-PROP / C_y=R C_x R^T",
     [("p3_rsmp_covariance.cpp",
       "    for (int o = 0; o < no; ++o) {\n      cy(o, p) += m(o, i) * w;\n    }",
       "    cy(p, p) += m(p, i) * w;  // MUTATION: diagonal-only")]),
    ("m_delta_psf", "ALG-P3-007 pi=S p",
     [("p3_rsmp_propagation.cpp",
       "  out.pi = apply_operator(s, in.psf_p);",
       "  out.pi.assign(static_cast<std::size_t>(s.n_out), 0.0);\n  out.pi[0] = 1.0;  // MUTATION: delta PSF")]),
    ("m_zero_fill_missing_tile", "C-P3-PROP-6 / 缺 tile 禁零填",
     [("p3_rsmp_operator.cpp",
       "    // 缺 tile/越界：fail-closed，输出 NaN，且**不零填**（C-P3-PROP-6）\n    if (n.missing_contributors > 0) continue;",
       "    // MUTATION: accept partial coverage as if present (zero-fill semantics)\n    if (false) continue;")]),
    ("m_mode_not_declared_gate_off", "FZ-P3-MODES",
     [("p3_rsmp_failclosed.cpp", "  if (!rec.mode_declared) {", "  if (false) {")]),
    ("m_bunit_quadratic_always_ok", "FZ-P3-BUNIT-QUADRATIC",
     [("p3_rsmp_units.cpp",
       "  const Bunit sq = bunit_square(signal);\n  return sq.adu_power == variance.adu_power && sq.px_power == variance.px_power;",
       "  (void)signal;\n  (void)variance;\n  return true;  // MUTATION: assume quadratic")]),
    ("m_sb_no_omega_gate_off", "G-P3-SB-01",
     [("p3_rsmp_failclosed.cpp",
       "    if (rec.flux_conversion && !rec.omega_per_pixel) {",
       "    if (false) {")]),
    ("m_psf_norm_gate_off", "G-P3-PSF-02",
     [("p3_rsmp_failclosed.cpp",
       "    } else if (!(std::fabs(rec.psf_sum - 1.0) <= cfg.psf_sum_tol)) {",
       "    } else if (false) {")]),
    ("m_psf_pointinfo_gate_off", "G-P3-PSF-03",
     [("p3_rsmp_failclosed.cpp",
       "    if (!rec.point_information_present && !rec.rebuildable_from_frames) {",
       "    if (false) {")]),
    ("m_psf_scale_gate_off", "G-P3-PSF-05",
     [("p3_rsmp_failclosed.cpp",
       "    if (!rec.photometric_scale_present || !(rec.photometric_scale > 0.0) ||\n        std::isnan(rec.photometric_scale)) {",
       "    if (false) {")]),
    ("m_vis_measurement_gate_off", "G-P3-VIS-01",
     [("p3_rsmp_failclosed.cpp",
       "    if (rec.measurement_capable || rec.writes_variance || rec.writes_ivar ||\n        rec.writes_point_information) {",
       "    if (false) {")]),
    ("m_median_snr_weight_allowed", "FZ-GATE-MEDIAN-SNR / C-004.2",
     [("p3_rsmp_failclosed.cpp",
       "  const auto& v = forbidden_weight_source_tokens();\n  return std::find(v.begin(), v.end(), token) != v.end();",
       "  (void)token;\n  return false;  // MUTATION: allow diagnostic weights")]),
    ("m_eps_corr_bit_corr_default_ratified", "CF-T-P3-CORR-EPSILON (OPEN)",
     [("p3_rsmp_failclosed.cpp",
       "  if (rec.covariance == CovarianceRepresentation::ApproximateCorrelation &&\n      !cfg.epsilon_corr_ratified) {",
       "  if (false) {"),
      ("p3_rsmp_failclosed.cpp",
       "  if (rec.correlation_approx_error_available && !cfg.epsilon_corr_ratified) {",
       "  if (false) {")]),
    ("m_qw04_optimism_gate_off", "G-P3-QW-04",
     [("p3_rsmp_failclosed.cpp",
       "  if (rec.covariance == CovarianceRepresentation::DiagonalOnly &&\n      (!rec.optimism_audit_performed || rec.diagonal_optimism_detected)) {",
       "  if (false) {")]),
    ("m_qw01_resampled_input_allowed", "FZ-P3-QW-RECOMPUTE / G-P3-QW-01",
     [("p3_rsmp_failclosed.cpp",
       "  if (rec.input_qw_resampled) {",
       "  if (false) {")]),
    ("m_provenance_gate_off", "FZ-PROV-MINIMAL-SET / G-P3-PROV-01",
     [("p3_rsmp_failclosed.cpp",
       "  if (!rec.provenance_complete || !rec.provenance_missing_keys.empty()) {",
       "  if (false) {")]),
    ("m_nearest_continuous_allowed", "FZ-P3-KERNEL-REGISTRY / G-P3-KRN-02",
     [("p3_rsmp_kernel_registry.cpp",
       '  if (kernel_id == "nearest" && use == KernelUse::ContinuousField) {',
       "  if (false) {"),
      ("p3_rsmp_kernel_registry.cpp",
       "  if (!k->allows(use)) {",
       "  if (false) {")]),
    ("m_registration_no_oracle_ok", "FZ-P3-KERNEL-REGISTRY / G-P3-KRN-03",
     [("p3_rsmp_kernel_registry.cpp",
       "  // 注册前置：唯一 id / 用途 / 语义 / 误差界 / 边界 / 独立 Oracle / 负向 mutation。",
       "  // MUTATION: skip registration structure checks entirely\n  return GateResult{Status::Ok, \"\", \"\"};")]),
    ("m_parse_mode_legacy_allowed", "FZ-MODE-DEFERRED / legacy 词表",
     [("p3_rsmp_units.cpp",
       "  // legacy / deferred / 未知 → REJECT（FZ-P3-MODES / FZ-MODE-DEFERRED / C-004.1）\n  return Status::Reject;",
       "  return Status::Ok;  // MUTATION: accept legacy/deferred")]),
]


def run(cmd, cwd=None):
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=600)
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return 124, "TIMEOUT"


def stage(dst):
    os.makedirs(dst, exist_ok=True)
    for name in os.listdir(LIB):
        if name.endswith(".cpp") or name.endswith(".h"):
            shutil.copy(os.path.join(LIB, name), os.path.join(dst, name))


def apply_patches(dst, patches):
    for rel, old, new in patches:
        path = os.path.join(dst, rel)
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
        n = text.count(old)
        if n != 1:
            raise RuntimeError("patch anchor matched %d times in %s: %r" % (n, rel, old[:60]))
        with open(path, "w", encoding="utf-8") as f:
            f.write(text.replace(old, new, 1))


def build_and_run(tag, libdir):
    outdir = os.path.join(BIN, tag)
    os.makedirs(outdir, exist_ok=True)
    results = {}
    details = []
    for t in TESTS:
        exe = os.path.join(outdir, t)
        src = os.path.join(HERE, t + ".cpp")
        cxx = ["g++"] + CXXFLAGS + ["-I", libdir, "-I", HERE, src]
        cxx += [os.path.join(libdir, f) for f in sorted(os.listdir(libdir)) if f.endswith(".cpp")]
        cxx += ["-o", exe]
        rc, log = run(cxx)
        if rc != 0:
            results[t] = ("compile_fail", rc)
            details.append("%s COMPILE_FAIL rc=%d\n%s" % (t, rc, log[-2000:]))
            continue
        rc, log = run([exe])
        results[t] = ("pass" if rc == 0 else "fail", rc)
        details.append("%s rc=%d %s" % (t, rc, log.strip().splitlines()[-1] if log.strip() else ""))
    return results, "\n".join(details)


def main():
    os.makedirs(MUT, exist_ok=True)
    os.makedirs(LOGS, exist_ok=True)
    summary = {"task": "IMPL-P3-RSMP-001", "pristine": None, "mutations": []}

    # 1) 原始（未注入）影子副本：必须全绿
    pristine = os.path.join(MUT, "pristine", "lib")
    stage(pristine)
    res, log = build_and_run("pristine", pristine)
    pristine_ok = all(v[0] == "pass" for v in res.values())
    summary["pristine"] = {"results": res, "ok": pristine_ok}
    with open(os.path.join(LOGS, "20_mutation_pristine.log"), "w", encoding="utf-8") as f:
        f.write(log + "\n")
    print("[pristine] %s -> %s" % (res, "OK" if pristine_ok else "UNEXPECTED"))

    # 2) 每条 mutation：必须至少一个测试变红
    all_ok = pristine_ok
    for mid, node, patches in MUTATIONS:
        dstdir = os.path.join(MUT, mid, "lib")
        stage(dstdir)
        try:
            apply_patches(dstdir, patches)
        except RuntimeError as exc:
            summary["mutations"].append({"id": mid, "frozen_node": node, "applied": False,
                                         "error": str(exc), "caught": False})
            print("[%s] PATCH ERROR: %s" % (mid, exc))
            all_ok = False
            continue
        res, log = build_and_run(mid, dstdir)
        compile_fail = any(v[0] == "compile_fail" for v in res.values())
        caught = (not compile_fail) and any(v[0] == "fail" for v in res.values())
        summary["mutations"].append({
            "id": mid,
            "frozen_node": node,
            "applied": True,
            "compile_fail": compile_fail,
            "results": {k: v[0] for k, v in res.items()},
            "caught_red": caught,
        })
        with open(os.path.join(LOGS, "21_mutation_%s.log" % mid), "w", encoding="utf-8") as f:
            f.write(log + "\n")
        status = "CAUGHT(red)" if caught else ("COMPILE_FAIL" if compile_fail else "NOT-CAUGHT")
        print("[%s] %s (%s)" % (mid, status, node))
        if not caught:
            all_ok = False

    n_caught = sum(1 for m in summary["mutations"] if m.get("caught_red"))
    summary["counts"] = {"mutations": len(MUTATIONS), "caught": n_caught,
                         "pristine_ok": pristine_ok}
    with open(os.path.join(WORK, "mutation_summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=1, ensure_ascii=False)
    print("SUMMARY: %d/%d mutations red; pristine_ok=%s -> %s" %
          (n_caught, len(MUTATIONS), pristine_ok, "PASS" if all_ok else "FAIL"))
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
