#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 表格生成：把 e1..e5 的 JSON 汇总成 results/EXP05_TABLES.md（自动生成，勿手改）。"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp05_common as X  # noqa: E402


def load(p: Path) -> Dict[str, Any]:
    return json.loads(p.read_text(encoding="utf-8"))


def f(x, n=4):
    if x is None:
        return "n/a"
    try:
        v = float(x)
    except (TypeError, ValueError):
        return str(x)
    if v != v:
        return "nan"
    return ("%+." + str(n) + "f") % v


def pct(x, n=2):
    if x is None:
        return "n/a"
    return ("%+." + str(n) + "f%%") % (100.0 * float(x))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "EXP05_TABLES.md"))
    a = ap.parse_args()
    e1 = load(X.RESULTS / "exp05_e1_analytic.json")
    e2 = load(X.RESULTS / "exp05_e2_hst.json")
    e3 = load(X.RESULTS / "exp05_e3_real.json")
    e4 = load(X.RESULTS / "exp05_e4_weight.json")
    e5 = load(X.RESULTS / "exp05_e5_gates.json")
    L: List[str] = []
    L.append("# EXP-05 对照表（自动生成，勿手改）")
    L.append("")
    L.append("生成脚本：`实验/absolute-snr/code/exp05/make_tables.py`；"
             "原始 JSON：`exp05_e{1,2,3,4,5}_*.json`。")
    L.append("")
    L.append("> 口径：SNR = F_ref/(sigma*sqrt(A_NEA))，报告取 F_ref=1、A_NEA=1；")
    L.append("> **SNR 相对偏差 = 1/(sigma 比值) - 1**（严格）。负值 = 重建 SNR 偏低。")
    L.append("")
    # ---- 表 A：三类数据 × 两种表示 ----
    L.append("## 表 A 绝对表示 vs 相对表示的 SNR 电平偏差（三类数据，全部对真值）")
    L.append("")
    L.append("| 数据类 | 场景 | c_agg | b_f（帧级估计器） | b_c（局部估计器） | c_sigma | "
             "**绝对表示 SNR 偏差** | **相对表示 SNR 偏差** | 比值 |")
    L.append("|---|---|---|---|---|---|---|---|---|")
    for s in e1["scenarios"]:
        ratio = (abs(s["rel_snr_dev_R0"]) / abs(s["abs_snr_dev_R0"])
                 if s["abs_snr_dev_R0"] else float("nan"))
        L.append("| 解析合成 | %s | %s | %s | %s | %s | **%s** | **%s** | %s |"
                 % (s["scenario"], f(s["c_agg"]), f(s["b_frame"]), f(s["b_patch_R0"]),
                    f(s["c_sigma_R0"]), pct(s["abs_snr_dev_R0"]),
                    pct(s["rel_snr_dev_R0"]), ("%.1fx" % ratio) if ratio == ratio else "n/a"))
    for s in e2["scenarios"]:
        ratio = (abs(s["rel_snr_dev_R0"]) / abs(s["abs_snr_dev_R0"])
                 if s["abs_snr_dev_R0"] else float("nan"))
        L.append("| HST 前向 | %s | %s | %s | %s | %s | **%s** | **%s** | %s |"
                 % (s["scenario"], f(s["c_agg"]), f(s["b_frame"]), f(s["b_patch_R0"]),
                    f(s["c_sigma_R0"]), pct(s["abs_snr_dev_R0"]),
                    pct(s["rel_snr_dev_R0"]), ("%.1fx" % ratio) if ratio == ratio else "n/a"))
    for p in e3["panels"]:
        wa = max(abs(x) for x in p["abs_snr_dev_per_frame"])
        wr = max(abs(x) for x in p["rel_snr_dev_per_frame"])
        L.append("| testdata 真实 | panel %s（%d 帧，逐帧见下） | n/a | n/a | n/a | %s~%s | **<= %s** | **<= %s** | %.1fx |"
                 % (p["panel"], p["n_frames"],
                    f(min(p["c_sigma_per_frame"]), 3), f(max(p["c_sigma_per_frame"]), 3),
                    pct(wa), pct(wr), (wr / wa) if wa else float("nan")))
    L.append("")
    L.append("**读法**：绝对表示的电平误差 = 局部估计器自身的误差（b_c）；")
    L.append("相对表示 = c_sigma x b_c，其中 c_sigma = c_agg x b_f / b_c。")
    L.append("当 c_agg、b_f 明显偏离 1（结构污染 / sigma 场空间离散）时，相对表示被成倍放大。")
    L.append("")
    # ---- 表 A2：真实数据逐帧 ----
    L.append("### 表 A2 testdata 真实数据逐帧明细（1px 棋盘 hold-out 真值，零像素重叠）")
    L.append("")
    L.append("| panel | 文件 | sigma_frame | sigma_patch 中位 | sigma_true 中位 | c_sigma | "
             "绝对 SNR 偏差 | 相对 SNR 偏差 | 真值自身 SE |")
    L.append("|---|---|---|---|---|---|---|---|---|")
    for r in e3["frames"]:
        L.append("| %s | %s | %.4f | %.4f | %.4f | %.4f | %s | %s | %.4f |"
                 % (r["panel"], r["file"][:46], r["sigma_frame_fam0"],
                    r["sigma_patch_median_fam0"], r["sigma_true_patch_median_fam1"],
                    r["c_sigma"], pct(r["abs_snr_dev"]), pct(r["rel_snr_dev"]),
                    r["truth_se_median"]))
    L.append("")
    # ---- 表 B：跨帧 ----
    L.append("## 表 B 跨帧比值偏差（SCI-B 核心目标）")
    L.append("")
    L.append("| 数据类 | 场景 / panel | c_k | 绝对表示 跨帧 SNR 偏差 | 相对表示 跨帧 SNR 偏差 |")
    L.append("|---|---|---|---|---|")
    for c in e1["cross_frame"]["cases"]:
        L.append("| 解析合成 | %s | %s | %s | **%s** |"
                 % (c["case"], ["%.3f" % x for x in c["c_sigma_median"]],
                    pct(c["abs_snr_dev"]), pct(c["rel_snr_dev"])))
    for s in e2["scenarios"]:
        if s["scenario"] == "hst_null":
            continue
        xa = max(abs(v["snr_rel_dev"]) for v in s["cross_frame_abs_R0"].values())
        xr = max(abs(v["snr_rel_dev"]) for v in s["cross_frame_rel_R0"].values())
        L.append("| HST 前向 | %s | %s | %s | **%s** |"
                 % (s["scenario"], ["%.3f" % x for x in s["c_sigma_R0_per_frame"]],
                    pct(xa), pct(xr)))
    for p in e3["panels"]:
        if not p.get("cross_frame_abs"):
            continue
        xa = max(abs(v["snr_rel_dev"]) for v in p["cross_frame_abs"].values())
        xr = max(abs(v["snr_rel_dev"]) for v in p["cross_frame_rel"].values())
        L.append("| testdata 真实 | panel %s | %s | %s | **%s** |"
                 % (p["panel"], ["%.3f" % x for x in p["c_sigma_per_frame"]],
                    pct(xa), pct(xr)))
    L.append("")
    # ---- 表 C：权重 ----
    L.append("## 表 C 权重视角（共模相消定理 + 跨帧反例）")
    L.append("")
    d1 = e4["D1_theorem"]
    L.append("### C1 定理的数值验证（正齐次算子）")
    L.append("")
    L.append("| 量 | 实测 | 判据 |")
    L.append("|---|---|---|")
    for k, v in d1["operator_scale_equivariance_max_rel"].items():
        L.append("| 算子 %s 的尺度等变性 R[a*v]==a*R[v] | %.2e | <= 1e-12 |" % (k, v))
    L.append("| 单帧归一化加权均值 两表示相对差 | %.3e | <= 1e-12 |"
             % d1["single_frame_mean_rel_diff"])
    L.append("| 权重效率 E 两表示绝对差 | %.3e | <= 1e-12 |"
             % d1["weight_efficiency_rel_diff"])
    L.append("")
    L.append("### C2 跨帧：c_k 不同 ⇒ 权重效率损失与组合电平偏差")
    L.append("")
    L.append("| 来源 | c_k | 权重效率损失 E | 组合 SNR 偏差（总） | 共模项 | 跨帧项 |")
    L.append("|---|---|---|---|---|---|")
    for c in e4["D2_cross_frame"]["cases"]:
        L.append("| %s | %s | %.4f | %s | %s | %s |"
                 % (c["case"], ["%.3f" % x for x in c["c"]], c["E"],
                    pct(c["combined_snr_rel_dev"]),
                    pct(c["combined_snr_dev_common_only"]),
                    pct(c["combined_snr_dev_cross_frame_only"])))
    L.append("")
    L.append("### C3 反例：定理条件（正齐次）不成立时")
    L.append("")
    L.append("| 算子 | 非共模残差 max | 非共模残差 rms |")
    L.append("|---|---|---|")
    for k, v in e4["D3_counterexample"]["non_common_mode_residual"].items():
        L.append("| %s | %.3e | %.3e |" % (k, v["max_rel"], v["rms_rel"]))
    L.append("")
    # ---- 表 E：机理核对（无需真值）----',
    try:
        e6 = load(X.RESULTS / "exp05_e6_mechanism.json")
    except Exception:
        e6 = None
    if e6:
        L.append("## 表 E 机理核对：帧级 sigma 被大尺度结构撑大多少（真实数据，**无需真值**）")
        L.append("")
        L.append("同一 recipe（整帧 2 轮裁剪 RMS）在**原始像素** vs **减去 64px mesh 局部背景后的残差**上的读数之比。")
        L.append("")
        L.append("| panel | 文件 | 原始整帧 sigma | 去结构后 sigma | 结构倍数 |")
        L.append("|---|---|---|---|---|")
        for r in e6["frames"]:
            L.append("| %s | %s | %.3f | %.3f | **x%.3f** |"
                     % (r["panel"], r["file"][:44], r["sigma_frame_raw"],
                        r["sigma_frame_struct_removed"], r["structure_boost"]))
        L.append("")
    # ---- 表 D：门 ----
    L.append("## 表 D 判据（能红能绿）")
    L.append("")
    L.append("| 判定 | 判据 | 实测 | 期望 |")
    L.append("|---|---|---|---|")
    for g in e5["gates"]:
        v = g["value"]
        vs = json.dumps(v, ensure_ascii=False) if not isinstance(v, (int, float)) else f(v, 6)
        L.append("| **%s** | %s | %s | %s |" % (g["verdict"], g["gate"], vs[:80], g["expect"]))
    L.append("")
    L.append("`ALL_GATES_PASS = %s`（%d 条）" % (e5["meta"]["all_pass"], e5["meta"]["n_gates"]))
    L.append("")
    out = Path(a.out)
    out.write_text("\n".join(L) + "\n", encoding="utf-8")
    print("wrote", out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
