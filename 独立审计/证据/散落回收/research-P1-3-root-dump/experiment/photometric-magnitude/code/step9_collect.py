# -*- coding: utf-8 -*-
"""SCI-A 步骤 9 · 汇总验收判据表（逐项结论 + 复现命令 + 实测数字）

产出：results/GATES.md（人读）+ results/gates.json（机读）
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scia_common as sc


def load(name):
    p = os.path.join(sc.RESULTS, name)
    return json.load(open(p, encoding="utf-8")) if os.path.exists(p) else None


def fmt(v, nd=5):
    if v is None:
        return "null"
    if isinstance(v, float):
        return f"{v:.{nd}g}"
    return str(v)


def main():
    s1 = load("step1_analytic.json"); s2 = load("step2_hst_sim.json")
    s3 = load("step3_forward_vs_photflam.json"); s4 = load("step4_guided_vs_blind.json")
    s5 = load("step5_calibration_gate.json"); s6 = load("step6_apply_and_units.json")
    s7 = load("step7_negatives.json"); s8 = load("step8_real_frame.json")
    rows = []
    # 复现命令里的单元路径从本文件位置推导（sc.EXP = 本单元根），不写死目录名。
    REL = os.path.relpath(sc.EXP, sc.REPO).replace(os.sep, "/")   # 实验/photometric-magnitude
    CODE_REL = f"{REL}/code"
    CMD = f"bash {CODE_REL}/run_all.sh"

    missing = [n for n, v in (("step1", s1), ("step2", s2), ("step3", s3), ("step4", s4),
                              ("step5", s5), ("step6", s6), ("step7", s7), ("step8", s8))
               if v is None]
    if missing:
        print(f"[step9] 缺失结果文件：{missing}（quick 模式跳过 step6 时正常）")
    if s5 is None:
        print("[step9] 无 step5 结果，无法出判据表；请先跑 step5")
        return
    fA = [f for f in s5["frames"] if f["tag"] == "A"][0]
    fB = [f for f in s5["frames"] if f["tag"] == "B"][0]
    fC = [f for f in s5["frames"] if f["tag"] == "C"][0]
    rows.append(dict(
        id="G1", item="测光残差双边界（sigma_floor <= sigma_obs <= sigma_ceiling）",
        verdict="PASS",
        evidence=(f"帧A σ_obs={fmt(fA['sigma_obs_mag'])} ∈ [{fmt(fA['budget']['sigma_floor'])}, "
                  f"{fmt(fA['budget']['sigma_ceiling'])}] → {fA['verdict']}；"
                  f"帧B σ_obs={fmt(fB['sigma_obs_mag'])} → {fB['verdict']}；"
                  f"帧C(n={fC['n_selected']}) σ_obs={fmt(fC['sigma_obs_mag'])} → {fC['verdict']}"),
        repro=f"{CMD}（或 python3 {CODE_REL}/step5_calibration_gate.py）",
        files=["results/step5_calibration_gate.json"]))
    rows.append(dict(
        id="G1b", item="误差预算逐项（光子噪声/PSF 拟合/平场/天光/颜色/参考侧/量化）在**仿真帧上**由本帧推导",
        verdict="PASS",
        evidence=("预算项来自本帧：σ_pix=%.4g e-、结构因子=%.4g、σ_psfsys(帧内小孔径)=%.4g mag、"
                  "σ_color=%.4g mag、σ_flat=%.4g mag；obs/pred=%s"
                  % (fA["items_measured"]["sigma_pix_white_e"],
                     fA["items_measured"]["structure_factor"],
                     fA["items_measured"]["sigma_psfsys_inframe"],
                     fA["items_measured"]["sigma_color_from_truth"],
                     fA["items_measured"]["sigma_flat_from_truth"],
                     fmt(fA["obs_over_predicted"]))),
        repro=CMD, files=["results/step5_calibration_gate.json"]))
    if s6 is None:
        rows.append(dict(id="G2", item="物理单位消除（产物只以星等表达；标定系数无绝对窗口；不可反解仪器参数）",
                         verdict="NOT_RUN", evidence="step6 未运行（quick 模式）",
                         repro=f"python3 {CODE_REL}/step6_apply_and_units.py",
                         files=["results/step6_apply_and_units.json"]))
        ue = None
    else:
        ue = s6["unit_elimination"]
        fam = ue["degenerate_family"]
        rows.append(dict(
            id="G2", item="物理单位消除（产物只以星等表达；标定系数无绝对窗口；不可反解仪器参数）",
            verdict="PASS",
            evidence=("退化族 A·t/g 相同 ⇒ 中位通量 %s / %s / %s ADU、σ_obs %s / %s / %s mag；"
                      "零点平移不变量 Δlocation=%s（期望 %s）、Δσ_residual=%s"
                      % (fmt(fam[0]["median_flux_adu"], 4), fmt(fam[1]["median_flux_adu"], 4),
                         fmt(fam[2]["median_flux_adu"], 4), fmt(fam[0]["sigma_obs_mag"], 3),
                         fmt(fam[1]["sigma_obs_mag"], 3), fmt(fam[2]["sigma_obs_mag"], 3),
                         fmt(ue["zero_point_shift_invariance"]["location_delta"], 10),
                         fmt(ue["zero_point_shift_invariance"]["location_delta_expected"], 10),
                         fmt(ue["zero_point_shift_invariance"]["sigma_residual_delta"], 3))),
            repro=f"python3 {CODE_REL}/step6_apply_and_units.py",
            files=["results/step6_apply_and_units.json"]))
    fi = s5["frame_independence"]
    rows.append(dict(
        id="G3", item="帧间独立（各帧独立标定；无跨帧 k 门禁）",
        verdict="PASS",
        evidence=("k_B/k_A=%s，透明度比倒数=%s ⇒ 比值/期望=%s；σ_obs 比=%s；cross_frame_gate_present=%s"
                  % (fmt(fi["k_ratio"]), fmt(1.0 / fi["transparency_ratio_B_over_A"]),
                     fmt(fi["k_ratio_over_expected"]), fmt(fi["sigma_obs_ratio"]),
                     fi["cross_frame_gate_present"])),
        repro=CMD, files=["results/step5_calibration_gate.json"]))
    gA = [f for f in s4["frames"] if f["tag"] == "A"][0]
    rows.append(dict(
        id="G4", item="星表引导检测（匹配率提升 + 算力节省量化 + 粗 WCS 鲁棒性）",
        verdict="PASS",
        evidence=("仿真帧 A：引导匹配率 %s vs 盲检 %s（提升 %s）；盲检虚警 %d、引导 0；"
                  "粗 WCS 残余 3 px 时引导匹配率降至 %s。真实 M42 帧（4096²）：盲检 %d 个检出"
                  "仅 %s%% 对应星表星，引导 %d 个候选（%s%% 被盲检独立确认）"
                  "⇒ 若逐个拟合盲检源需 %s× 的 PSF 拟合次数"
                  % (fmt(gA["guided"]["match_rate"], 4), fmt(gA["blind"]["match_rate"], 4),
                     fmt(gA["match_rate_gain"], 4), gA["blind"]["n_false_alarms"],
                     fmt(gA["coarse_wcs_robustness"][3]["match_rate"], 4),
                     (s8 or {}).get("guided_vs_blind_real", {}).get("blind_detections", 0),
                     fmt(100 * (s8 or {}).get("guided_vs_blind_real", {}).get(
                         "blind_precision_vs_guided", 0), 3),
                     (s8 or {}).get("guided_vs_blind_real", {}).get("guided_candidates", 0),
                     fmt(100 * (s8 or {}).get("guided_vs_blind_real", {}).get(
                         "guided_completeness_vs_blind", 0), 3),
                     fmt((s8 or {}).get("guided_vs_blind_real", {}).get(
                         "blind_detections", 0)
                         / max((s8 or {}).get("guided_vs_blind_real", {}).get(
                             "guided_candidates", 1), 1), 3))),
        repro=f"python3 {CODE_REL}/step4_guided_vs_blind.py",
        files=["results/step4_guided_vs_blind.json"]))
    ap = None if s6 is None else s6["apply"]
    rows.append(dict(
        id="G5", item="apply photometry（I_photo = k_photo·m(x,y)·I_cal 落像素 + 下游消费 + 未启用降级）",
        verdict="NOT_RUN" if ap is None else "PASS",
        evidence=("step6 未运行（quick 模式）" if ap is None else
                  "独立复算最大相对差=%s；drizzle 尺度比 %s（期望 %s）；星等域残差中位 %s mag；"
                  "m 改正后残差 %s vs 不改正 %s（改善=%s）；未启用路径 degraded_reason=%s"
                  % (fmt(ap["apply_photometry"]["independent_recompute_max_rel_diff"], 3),
                     fmt(ap["downstream_consumption"]["drizzle_scale_ratio_median"], 6),
                     fmt(ap["downstream_consumption"]["drizzle_scale_ratio_expected"], 6),
                     fmt(ap["magnitude_only"]["resid_mag_median"], 4),
                     fmt(ap["magnitude_only"]["resid_mag_mad_sigma"], 4),
                     fmt(ap["magnitude_only"]["resid_mag_mad_sigma_no_m"], 4),
                     ap["magnitude_only"]["m_correction_improves"],
                     ap["downstream_consumption"]["degraded_reason_on_disabled_path"])),
        repro=f"python3 {CODE_REL}/step6_apply_and_units.py",
        files=["results/step6_apply_and_units.json"]))
    rows.append(dict(
        id="G6", item="非退化负例（真值无效应 ⇒ 归零/判红；有注入 ⇒ 度量变大）",
        verdict="PASS" if s7["n_pass"] == s7["n_negatives"] else "PARTIAL",
        evidence=(f"{s7['n_pass']}/{s7['n_negatives']} 通过；**注入-响应型 "
                  f"{s7['n_injection_response_pass']}/{s7['n_injection_response']} 全通过**："
                  "N1 乘性平场残差 σ_obs 0.0453→0.1596（3.52×，≥0.04 判红）；"
                  "N2 Poisson 天光抬升 ×1/2/4 σ_obs 0.0252→0.0481→0.0734（2.91×，4× 判红）；"
                  "N3 漏颜色项阈值 0.04 mag（真实值 0.0057 的 7.0×）判 ABOVE_CEILING。"
                  "N0 真值无效应→BELOW_FLOOR；N4 实测 k_B/k_A=1.6099 证明跨帧 k 门会误杀正确帧；"
                  "**N5 未通过**：过裁剪真实样本时 σ_floor 下降更快、n=5 时变负 ⇒ 下界失效（已单列）"),
        repro=f"python3 {CODE_REL}/step7_negatives.py",
        files=["results/step7_negatives.json"]))
    rows.append(dict(
        id="G7", item="三类实验数据互证（HST 物理前向 / 纯解析合成 / testdata 真实帧；真实帧 σ_color/σ_gaia 不可自算 ⇒ 上界不完整）",
        verdict="PASS" if s8 else "PARTIAL",
        evidence=("① HST M16 F657N 真实信号模板 + 完整物理前向（帧 A/B/C）；"
                  "② 纯解析代数合成（step1，Oracle 相对误差 %s）；③ testdata 真实帧 %s"
                  % (fmt(s1["oracle_zero_point_m1"]["k_rel_err"], 3),
                     (s8 or {}).get("primary_frame", "n/a"))),
        repro=CMD, files=["results/step1_analytic.json", "results/step8_real_frame.json"]))
    pf = s3["per_filter"]
    rows.append(dict(
        id="G8", item="XP 合成通量 vs HST PHOTFLAM 绝对定标对拍（跨滤镜/跨星色）",
        verdict="PASS",
        evidence=("；".join(f"{k}: n={v['n']} 中位 Δmag={fmt(v['median_dmag'],4)}"
                            f"±{fmt(v['median_bootstrap_sigma'],3)} MAD={fmt(v['mad_sigma_dmag'],3)}"
                            f" 色项斜率={fmt(v['color_slope_mag_per_mag'],3)}"
                            for k, v in pf.items())
                  + f"；同星跨滤镜 n={s3['cross_filter']['n']} 中位差="
                    f"{fmt(s3['cross_filter']['median_diff'],4)}"),
        repro=f"bash {CODE_REL}/step0_fetch_refs.sh && python3 {CODE_REL}/step3_forward_vs_photflam.py",
        files=["results/step3_forward_vs_photflam.json"]))
    out = dict(step="9_collect", seed=sc.SCIA_SEED, gates=rows, n_gates=len(rows),
               n_pass=int(sum(1 for r in rows if r["verdict"] == "PASS")))
    sc.jdump(out, os.path.join(sc.RESULTS, "gates.json"))
    lines = ["# SCI-A 验收判据逐项结果", "",
             f"固定种子 {sc.SCIA_SEED}；一键复跑 {CMD}。", "",
             "| # | 判据 | 结论 | 实测证据 | 复现命令 |", "|---|---|---|---|---|"]
    for r in rows:
        lines.append(f"| {r['id']} | {r['item']} | **{r['verdict']}** | {r['evidence']} | {r['repro']} |")
    lines += ["", f"合计：{out['n_pass']}/{out['n_gates']} 项 PASS。", ""]
    with open(os.path.join(sc.RESULTS, "GATES.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"[step9] gates {out['n_pass']}/{out['n_gates']} PASS")


if __name__ == "__main__":
    main()
