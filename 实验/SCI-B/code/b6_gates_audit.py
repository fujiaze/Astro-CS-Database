#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B / B6：退化门审查 + fail-closed 路径对拍 + 非退化自检。

假说：
  H1 「帧级臂 RMSE 恒等于 s_field」类判据是恒真门：RMSE(constant, median-normalized truth)
     在数学上恒等于 truth 场的中位归一离散度 ⇒ RMSE ≤ K·s_field（K=1.25）永远为真，
     对任何数据（含对抗性打乱场）都绿 ⇒ 不携带信息、不作证据。
  H2 替代判据 = 权重效率损失 E=Var_w/Var_opt−1：E=0 ⇔ σ̂ ∝ σ_true（全局尺度相消）；
     +10% 电平注入必须使 E 变红，无注入必须绿（双向可假）。
  H3 误差预算常数单位审查：NOISE_MODEL.md:86 的 1.44/√N 是**相对**标准误；
     E11 把 c_est=1.44 直接当作 **dex** 阈值使用 ⇒ 噪声项高估 ln10≈2.303 倍。
  H4 SNR 重建路径 fail-closed：无稀疏层⇒按帧级执行但必须显式记 snr_path_effective；
     稀疏层损坏⇒显式失败；静默降级变体必须被审计判红。
输出：results/b6_gates_audit.json
"""
from __future__ import annotations

import argparse
import math
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C  # noqa: E402

N_PATCH = 64
K_GATE = 1.25
N_MC = 2000


def tautology_demo(seed_off=0):
    r = C.rng(seed_off)
    rows = []
    for tag, truth in [
        ("smooth_grf", 10 ** (0.10 * r.normal(size=N_PATCH * N_PATCH))),
        ("adversarial_scramble", 10 ** (0.30 * r.permutation(np.linspace(-2, 2, N_PATCH * N_PATCH)))),
        ("flat", np.ones(N_PATCH * N_PATCH)),
    ]:
        truth = np.asarray(truth, float)
        # 帧级臂 = 常数场 = truth 的中位数（最佳常数）
        est = np.full_like(truth, np.median(truth))
        a = est / np.median(est); b = truth / np.median(truth)
        rmse = float(np.sqrt(np.mean((np.log10(a) - np.log10(b)) ** 2)))
        s_field = float(np.std(np.log10(b)))
        rows.append(dict(case=tag, rmse_frame=rmse, s_field=s_field,
                         gate_tau=K_GATE * s_field, gate_green=bool(rmse <= K_GATE * s_field),
                         ratio_rmse_over_sfield=float(rmse / s_field) if s_field > 0 else None,
                         # 同样的常数场在"权重效率"意义下的损失
                         eff_loss=C.weight_efficiency_loss(est, truth, np.ones(truth.size, bool))))
    return dict(K=K_GATE, rows=rows,
                all_green=bool(all(x["gate_green"] for x in rows)),
                note="RMSE(constant, median-normalized truth) 恒等于 std(log10 truth) ⇒ 恒真门")


def eff_loss_fault_injection(seed_off=0):
    """替代判据的双向可假：无注入绿、+10% 电平注入红、真值无效应归零。"""
    r = C.rng(seed_off)
    truth = 10 ** (0.12 * r.normal(size=N_PATCH * N_PATCH))
    out = []
    for tag, factor in [("unbiased", 1.0), ("uniform_scale_1.10_E_must_be_0", 1.10),
                        ("split_half_pm10pct_multiplicative", None),
                        ("fault_shuffle", "shuffle"), ("null_sigma_propto_truth", "prop")]:
        if factor == "shuffle":
            est = truth[r.permutation(truth.size)]
        elif factor == "prop":
            est = truth * 1.0
        elif factor is None:
            est = truth * (1 + 0.10 * np.sign(np.log10(truth) - np.median(np.log10(truth))))
        else:
            est = truth * factor
        e = C.weight_efficiency_loss(est, truth, np.ones(truth.size, bool))
        out.append(dict(case=tag, eff_loss=float(e), red=bool(e > 0.01)))
    return out


def c_est_unit_audit():
    """E11 的 c_est=1.44 在 dex 阈值中把相对标准误当 dex 用 ⇒ 高估 ln10 倍。"""
    r = C.rng(99)
    n = 1024
    T = 4000
    x = r.normal(0, 1, size=(T, n))
    mad = C.K_MAD_TO_SIGMA * np.median(np.abs(x - np.median(x, axis=1, keepdims=True)), axis=1)
    rel_se_measured = float(mad.std(ddof=1))                  # 相对（无量纲）
    dex_se_measured = float(np.std(np.log10(mad)))            # dex
    c_rel_doc = 1.44 / math.sqrt(n)                           # NOISE_MODEL.md:86 口径（相对）
    c_dex_correct = 1.44 / math.log(10.0) / math.sqrt(n)      # 同一常数换算到 dex
    c_dex_e11 = 1.44 / math.sqrt(n)                           # E11 直接把 1.44 当 dex 常数
    return dict(n=n, n_mc=T,
                rel_se_measured=rel_se_measured, rel_se_formula_1p166=1.166 / math.sqrt(n),
                rel_se_doc_1p44=c_rel_doc, dex_se_measured=dex_se_measured,
                dex_se_correct_from_doc=c_dex_correct, dex_se_e11_used=c_dex_e11,
                e11_over_correct_factor=float(c_dex_e11 / c_dex_correct),
                measured_over_e11_factor=float(dex_se_measured / c_dex_e11))


def path_state_machine(path_cfg, has_sparse, sparse_valid, has_dense, silent=False):
    """SNR 重建路径状态机（07_noise_snr.md §4.2 语义）。返回 (status, effective, reason)。"""
    if path_cfg == "dense":
        if not has_dense:
            return ("fail", None, "FZ-SNR-DENSE-MISSING")
        return ("ok", "dense", None)
    if path_cfg == "sparse_reconstruct":
        if has_sparse and sparse_valid:
            return ("ok", "sparse_reconstruct", None)
        if has_sparse and not sparse_valid:
            return ("fail", None, "FZ-SNR-SPARSE-CORRUPT")
        # 无稀疏层：按帧级执行，但必须显式记录实际路径
        if silent:
            return ("ok", path_cfg, None)          # 故障注入：静默降级（伪装成请求路径）
        return ("ok", "frame_reconstruct", "SNR_PATH_FALLBACK_FRAME")
    if path_cfg == "frame_reconstruct":
        return ("ok", "frame_reconstruct", None)
    return ("fail", None, "FZ-SNR-PATH-UNKNOWN")


def fail_closed_audit():
    cases = [
        ("sparse_present", dict(path_cfg="sparse_reconstruct", has_sparse=True, sparse_valid=True,
                                has_dense=False), ("ok", "sparse_reconstruct")),
        ("sparse_absent_explicit_fallback", dict(path_cfg="sparse_reconstruct", has_sparse=False,
                                                 sparse_valid=False, has_dense=False),
         ("ok", "frame_reconstruct")),
        ("sparse_corrupt", dict(path_cfg="sparse_reconstruct", has_sparse=True, sparse_valid=False,
                                has_dense=False), ("fail", None)),
        ("dense_missing", dict(path_cfg="dense", has_sparse=False, sparse_valid=False,
                               has_dense=False), ("fail", None)),
        ("frame_path", dict(path_cfg="frame_reconstruct", has_sparse=False, sparse_valid=False,
                            has_dense=False), ("ok", "frame_reconstruct")),
    ]
    rows = []
    for name, kw, expect in cases:
        st, eff, reason = path_state_machine(**kw)
        rows.append(dict(case=name, status=st, effective=eff, reason=reason,
                         matches_expectation=bool((st, eff) == expect)))
    # 故障注入：静默降级必须被判红
    st, eff, reason = path_state_machine(path_cfg="sparse_reconstruct", has_sparse=False,
                                         sparse_valid=False, has_dense=False, silent=True)
    silent_detected = bool(not (st == "ok" and eff == "frame_reconstruct") and
                           not (reason is not None and "FALLBACK" in str(reason)))
    return dict(cases=rows, all_match=bool(all(x["matches_expectation"] for x in rows)),
                silent_variant=dict(status=st, effective=eff, reason=reason,
                                    detected_as_violation=silent_detected))


def self_test():
    """--self-test：判据非退化自检（正例必须绿、故障注入必须红），失败返回非零退出码。"""
    checks = []
    taut = tautology_demo()
    checks.append(("tautology_gate_is_vacuous(always green)", taut["all_green"] is True))
    eff = {x["case"]: x for x in eff_loss_fault_injection()}
    checks.append(("E: unbiased -> green", eff["unbiased"]["red"] is False))
    checks.append(("E: uniform scale -> cancels (0, by design)",
                   abs(eff["uniform_scale_1.10_E_must_be_0"]["eff_loss"]) < 1e-12))
    checks.append(("E: split-half +/-10% multiplicative -> RED",
                   eff["split_half_pm10pct_multiplicative"]["red"] is True))
    checks.append(("E: shuffled field -> RED", eff["fault_shuffle"]["red"] is True))
    checks.append(("E: sigma proportional to truth -> 0", abs(eff["null_sigma_propto_truth"]["eff_loss"]) < 1e-12))
    fc = fail_closed_audit()
    checks.append(("fail-closed (spec transcription only): all 5 path cases as specified",
                   fc["all_match"] is True))
    checks.append(("fail-closed (spec transcription only): silent fallback detected as violation",
                   fc["silent_variant"]["detected_as_violation"] is True))
    ce = c_est_unit_audit()
    checks.append(("c_est unit audit: dex overestimate factor == ln10",
                   abs(ce["e11_over_correct_factor"] - math.log(10.0)) < 1e-9))
    ok = all(v for _, v in checks)
    for name, v in checks:
        print("  [%s] %s" % ("PASS" if v else "FAIL", name))
    print("SELF-TEST:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "b6_gates_audit.json"))
    ap.add_argument("--self-test", action="store_true", help="只跑非退化自检（正例/负例注入）")
    a = ap.parse_args()
    if a.self_test:
        sys.exit(self_test())
    t0 = time.time()
    taut = tautology_demo()
    eff = eff_loss_fault_injection()
    c_est = c_est_unit_audit()
    fc = fail_closed_audit()
    gates = dict(
        H1_tautology_all_green=taut["all_green"],
        H1_tautology_rmse_within_5pct_of_sfield=bool(all(
            x["ratio_rmse_over_sfield"] is None or (0.999 <= x["ratio_rmse_over_sfield"] <= 1.05)
            for x in taut["rows"])),
        H1_adversarial_case_green_but_eff_loss=float(
            next(x["eff_loss"] for x in taut["rows"] if x["case"] == "adversarial_scramble")),
        H2_unbiased_green=bool(not eff[0]["red"]),
        H2_uniform_scale_cancels=bool(not eff[1]["red"]),
        H2_split_half_bias_red=bool(eff[2]["red"]),
        H2_shuffle_red=bool(eff[3]["red"]),
        H2_null_proportional_zero=bool(abs(eff[4]["eff_loss"]) < 1e-12),
        H3_e11_dex_constant_overestimates=bool(c_est["e11_over_correct_factor"] > 2.0),
        H3_factor=c_est["e11_over_correct_factor"],
        H3_note="因子按构造恰为 ln10（单位换算恒等式），属定义型检查，不携带数据信息",
        H4_spec_transcription_cases_match=fc["all_match"],
        H4_silent_fallback_detected=fc["silent_variant"]["detected_as_violation"],
        H4_scope_note=("按 docs/plugins/algorithms_phase1/07_noise_snr.md §4.2 转写的**语义自检**；"
                       "lib/ 实现侧尚无该状态机（无 sparse_reconstruct/snr_path_effective/FZ-SNR 符号），"
                       "因此不构成对实现的验证，也不得据此外推实现行为"),
    )
    obj = dict(experiment="SCI-B / B6 degenerate-gate audit, error-budget unit audit, fail-closed path audit",
               tautology=taut, eff_loss_injection=eff, c_est_audit=c_est, fail_closed=fc,
               gates=gates, generated_at=C.now(), wall_s=time.time() - t0)
    C.save_json(a.out, obj)
    print("wrote", a.out)
    for k, v in gates.items():
        print("  %-42s %s" % (k, v))
    print("  c_est:", c_est)
    print("  eff:", eff)


if __name__ == "__main__":
    main()
