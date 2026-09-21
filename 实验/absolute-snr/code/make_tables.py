#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B 对比表生成：从 results/*.json 汇总为 results/COMPARISON_TABLES.md（人读）。"""
from __future__ import annotations
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C


def L(name):
    with open(os.path.join(C.RESULTS, name), encoding="utf-8") as f:
        return json.load(f)


def main():
    b1, b2, b3, b4, b5, b6 = (L("b1_sky_scan.json"), L("b2_noise_terms.json"), L("b3_domain_map.json"),
                              L("b4_integration.json"), L("b5_phase3_transfer.json"), L("b6_gates_audit.json"))
    o = ["# SCI-B 对比表（由 results/*.json 自动生成，勿手改）", "",
         "生成命令：«BT»python3 实验/SCI-B/code/make_tables.py«BT»", ""]
    o += ["## T1 天光扫描（亮源 F=3000 e-，N_MC=1000）", "",
          "| B [e-/px] | SNR_def | SNR_MC | MC 95% CI | prod skyonly+RN | prod empirical+RN | prod empirical only | 传统口径 | 不扣背景 | 算术常数 |",
          "|---|---|---|---|---|---|---|---|---|---|"]
    for r in b1["sky_scan_bright"]:
        o.append("| %.0f | %.4f | %.4f | [%.3f, %.3f] | %.4f | %.4f | %.4f | %.1f | %.1f | %.6g |" % (
            r["sky_e_per_px"], r["snr_def"], r["snr_emp"], r["snr_emp_ci95"][0], r["snr_emp_ci95"][1],
            r["snr_prod_skyonly_rn"], r["snr_prod_empirical_rn"], r["snr_prod_empirical_only"],
            r["snr_arm_trad"], r["snr_arm_raw_frame"], r["snr_arm_const"]))
    o += ["", "## T2 暗源 F=100 e-（同结构）", "",
          "| B [e-/px] | SNR_def | SNR_MC | 95% CI | prod skyonly+RN | prod empirical+RN | 传统口径 |",
          "|---|---|---|---|---|---|---|"]
    for r in b1["sky_scan_faint"]:
        o.append("| %.0f | %.4f | %.4f | [%.3f, %.3f] | %.4f | %.4f | %.1f |" % (
            r["sky_e_per_px"], r["snr_def"], r["snr_emp"], r["snr_emp_ci95"][0], r["snr_emp_ci95"][1],
            r["snr_prod_skyonly_rn"], r["snr_prod_empirical_rn"], r["snr_arm_trad"]))
    o += ["", "## T3 sigma_F 噪声项扫描（定义 vs MC，29 点）", "",
          "| 扫描 | 轴值 | sigma_F 定义 | sigma_F MC | z_def | z(skyonly+RN) | z(emp+RN) | 双计预言 |",
          "|---|---|---|---|---|---|---|---|"]
    for scan, rows in b2["scans"].items():
        for r in rows:
            o.append("| %s | %s | %.4f | %.4f | %+.2f | %+.2f | %+.2f | %+.4f |" % (
                scan, r["axis_value"], r["sigma_f_def_adu"], r["sigma_f_mc_adu"], r["z_def_vs_mc"],
                r["z_skyonly_rn_vs_mc"], r["z_empirical_rn_vs_mc"], r["pred_doublecount_bias"]))
    o += ["", "## T4 三口径适用域（RMSE(log10 rho)，Delta=64；真实数据为 16 px hold-out 值）", "",
          "| 面 | ell_meas [px] | s_field | dense | sparse | frame_median | frame_mad | 稀疏-帧级 Delta* [px] |",
          "|---|---|---|---|---|---|---|---|"]
    faces = []
    for res in b3["faces"]["synthetic_grf"]:
        m = res["meta"]
        faces.append(("synth ell=%g s=%.2f" % (m["ell_nominal_px"], m["s_field_true_log10"]), res))
    faces.append(("hst_m16", b3["faces"]["hst_m16"]))
    for res in b3["faces"]["testdata_m42"]:
        faces.append((res["meta"]["file"][:24], res))
    for name, res in faces:
        def val(arm, _res=res):
            return next((r["rmse_log_rho"] for r in _res["rows"] if r["delta_px"] == 64 and r["arm"] == arm), float("nan"))
        o.append("| %s | %.1f | %.4f | %.4f | %.4f | %.4f | %.4f | %s |" % (
            name, res["ell_px"], res.get("s_field_log10_std", float("nan")), val("dense"), val("sparse"),
            val("frame_median"), val("frame_mad"), res["delta_star"]["delta_star_px"]))
    o += ["", "## T5 存储代价（4096^2/帧，float32；预算 1 MiB/帧）", "",
          "| 口径 | 字节/帧 | MiB/帧 | 预算倍数 |", "|---|---|---|---|"]
    cost = b3["faces"]["hst_m16"]["cost"]
    o.append("| dense | %d | %.2f | %.1f x |" % (cost["dense_bytes_4096"], cost["dense_MiB_4096"],
                                                cost["dense_bytes_4096"] / cost["budget_bytes"]))
    for d, by in sorted(cost["sparse_bytes_4096"].items(), key=lambda x: int(x[0])):
        o.append("| sparse Delta=%s | %d | %.5f | %.4f x |" % (d, by, by / 1048576, by / cost["budget_bytes"]))
    o.append("| frame | %d | %.6f | %.6f x |" % (cost["frame_bytes"], cost["frame_bytes"] / 1048576,
                                                cost["frame_bytes"] / cost["budget_bytes"]))
    o += ["", "## T6 逆方差集成对拍", "",
          "| 项 | 实测 | 解析/预言 | 相对差 |", "|---|---|---|---|"]
    pa, pc = b4["part_ab"]["mc"], b4["part_c"]["mc"]
    for tag, m, p in [("ivar 组合方差", pa["var_ivar"], pa["var_ivar_pred"]),
                      ("等权组合方差", pa["var_equal"], pa["var_equal_pred"]),
                      ("w∝SNR 组合方差", pa["var_snr_weight"], pa["var_snr_weight_pred"]),
                      ("Q/W 方差", pc["var_qw"], pc["var_qw_pred"]),
                      ("单帧方差", pc["var_single"], pc["var_single_pred"])]:
        o.append("| %s | %.2f | %.2f | %+.2f%% |" % (tag, m, p, 100 * (m / p - 1)))
    o.append("| SigmaSNR^2 恒等 | — | — | %.1e |" % b4["gates"]["H1_identity_rel_dev"])
    o.append("| 堆叠 1/sigma^2 vs 最优 | %.4f | %.4f | — |" % (b4["gates"]["H4_naive_suboptimal_ratio_measured"],
                                                          b4["gates"]["H4_naive_suboptimal_ratio_pred"]))
    o += ["", "### T6a F_ref 锚定适用域（锚定权重 scatter / oracle 权重 scatter）", "",
          "| m | 比值 |", "|---|---|"]
    for k, v in b4["gates"]["H5_anchored_ratio_by_mag"].items():
        o.append("| %s | %.4f |" % (k, v))
    o += ["", "### T6b F_ref 错配负例（ZP 散度 ⇒ 权重效率损失）", "",
          "| ZP 散度 [mag] | 效率损失 |", "|---|---|"]
    for x in b4["part_e"]["mismatch_negative"]:
        o.append("| %.1f | %.4f |" % (x["zp_spread_mag"], x["eff_loss"]))
    o += ["", "## T7 判据非退化审查", "",
          "| 用例 | 度量 | 判定 |", "|---|---|---|"]
    for x in b6["eff_loss_injection"]:
        o.append("| %s | E=%.4f | %s |" % (x["case"], x["eff_loss"], "红（预期）" if x["red"] else "绿"))
    o.append("| 恒真门（三种真值场，含对抗打乱） | 全绿 | 无证据资格（对抗场 E=%.2f） |" %
             b6["gates"]["H1_adversarial_case_green_but_eff_loss"])
    o.append("| c_est 单位（相对→dex） | 高估 %.3f x = ln10 | 订正登记 D2 |" % b6["gates"]["H3_factor"])
    o += ["", "## T8 Phase3 传递", "", "| 项 | 实测 | 解析 |", "|---|---|---|"]
    g5 = b5["gates"]
    o += ["| C_out 对角元 MC/解析 | %.4f | 1 |" % g5["H1_diag_ratio_mc_over_analytic"],
          "| 完整/对角方差 | %.3f | %.3f（1+0.75rho） |" % (g5["H2_full_over_diag_measured"], g5["H2_pred_1_plus_0p75rho"]),
          "| Var(F_hat_out) | %.3f | %.3f |" % (g5["H3_var_full_mc"], g5["H3_var_full_pred"]),
          "| 对角近似宣称/实际 | %.3f | — |" % g5["H3_diag_claim_over_actual"], ""]
    txt = "\n".join(o).replace("«BT»", chr(96))
    out = os.path.join(C.RESULTS, "COMPARISON_TABLES.md")
    with open(out, "w", encoding="utf-8") as f:
        f.write(txt)
    print("wrote", out, len(txt), "chars")


if __name__ == "__main__":
    main()
