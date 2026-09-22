#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-06 表格生成：把 results/exp06_*.json 渲染为 results/EXP06_TABLES.md。"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp06_common as X  # noqa: E402

METHOD_LABEL = {
    "frame_scalar": "frame_scalar（帧级标量）",
    "dense_patch": "dense_patch（块常数）",
    "interp_bilinear": "interp_bilinear（现行生产算子）",
    "interp_spline": "interp_spline（EXP-04 推荐算子）",
    "phys": "**phys（物理建模，推荐）**",
    "phys_resid": "phys_resid（物理+残差插值）",
    "phys_free": "phys_free（自由斜率）",
    "phys_auto": "**phys_auto（推荐默认：斜率可辨识则用数据，否则常数）**",
    "phys_auto_resid": "phys_auto_resid（推荐默认 + 残差插值）",
    "phys_unsep": "phys_unsep（不做结构分离，错误臂）",
    "phys_med3drv": "phys_med3drv（mesh 中值滤波驱动）",
    "phys_r0ctrl": "phys_r0ctrl（R0 控制值，错误臂）",
    "naive_pixel": "naive_pixel（逐像素代入亮度，错误臂）",
}
ORDER = ["frame_scalar", "dense_patch", "interp_bilinear", "interp_spline", "phys",
         "phys_resid", "phys_free", "phys_auto", "phys_auto_resid", "phys_med3drv",
         "phys_unsep", "phys_r0ctrl", "naive_pixel"]


def _load(p: Path) -> Optional[Dict[str, Any]]:
    if not p.exists():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


def _med(vals) -> float:
    v = [x for x in vals if x is not None and np.isfinite(x)]
    return float(np.median(v)) if v else float("nan")


def fmt(x, nd=5) -> str:
    if x is None or not np.isfinite(x):
        return "n/a"
    if abs(x) >= 1e4 or (x != 0 and abs(x) < 1e-4):
        return "%.2e" % x
    return ("%%.%df" % nd) % x


def table_arm_a(d: Dict[str, Any], out: List[str]) -> None:
    scenes: List[str] = []
    for f in d["frames"]:
        if f["scene"] not in scenes:
            scenes.append(f["scene"])
    for target, tname in (("vs_T1_var_bg", "T1 = var_bg（空背景口径，AstroCS 冻结口径）"),
                          ("vs_T2_var_local", "T2 = var_local（逐像素总方差口径）")):
        out.append("### A-%s 权重效率损失 E（中位；E=0 最优，尺度不变）\n" % tname.split(" ")[0])
        out.append("| 场景 | " + " | ".join(METHOD_LABEL.get(m, m) for m in ORDER) + " |")
        out.append("|---|" + "---|" * len(ORDER))
        for sc in scenes:
            fs = [f for f in d["frames"] if f["scene"] == sc]
            row = [_med([f["methods"].get(m, {}).get(target, {}).get("eff_loss") for f in fs])
                   for m in ORDER]
            out.append("| %s | %s |" % (sc, " | ".join(fmt(x) for x in row)))
        out.append("")
    out.append("### A-电平比 median(sigma_hat/sigma_true)（T1）\n")
    out.append("| 场景 | " + " | ".join(METHOD_LABEL.get(m, m) for m in ORDER) + " |")
    out.append("|---|" + "---|" * len(ORDER))
    for sc in scenes:
        fs = [f for f in d["frames"] if f["scene"] == sc]
        row = [_med([f["methods"].get(m, {}).get("vs_T1_var_bg", {}).get("level_ratio") for f in fs])
               for m in ORDER]
        out.append("| %s | %s |" % (sc, " | ".join(fmt(x, 4) for x in row)))
    out.append("")
    out.append("### A-源区失效模式（T1 口径；src_level_ratio 越接近 1 越好，src_snr_p05 越小越差）\n")
    out.append("| 场景 | 方法 | 源区 sigma 比 | 源区 SNR p05 | 全帧 SNR 最大比 |")
    out.append("|---|---|---|---|---|")
    for sc in scenes:
        fs = [f for f in d["frames"] if f["scene"] == sc]
        for m in ("phys", "interp_spline", "naive_pixel", "phys_unsep"):
            g = lambda k: _med([f["methods"].get(m, {}).get("vs_T1_var_bg", {}).get(k) for f in fs])
            if not np.isfinite(g("src_level_ratio")) and not np.isfinite(g("snr_max_all")):
                continue
            out.append("| %s | %s | %s | %s | %s |"
                       % (sc, m, fmt(g("src_level_ratio"), 3), fmt(g("src_snr_p05"), 3),
                          fmt(g("snr_max_all"), 3)))
    out.append("")
    out.append("### A-拟合诊断（中位）\n")
    out.append("| 场景 | 自由斜率 gain_hat | 偏差 vs 真值 1.3 | lever_var | c_se/c | 固定斜率 a | 固定斜率 R2 | 幂律 p |")
    out.append("|---|---|---|---|---|---|---|---|")
    for sc in scenes:
        fs = [f for f in d["frames"] if f["scene"] == sc]
        gh = _med([f["fits"]["free"].get("gain_hat") for f in fs])
        out.append("| %s | %s | %s | %s | %s | %s | %s | %s |"
                   % (sc, fmt(gh, 3), fmt(100 * (gh / 1.3 - 1.0), 1) + "%%",
                      fmt(_med([f["diag"]["lever_var"] for f in fs]), 3),
                      fmt(_med([f["fits"]["free"].get("c_se", float("nan"))
                                / max(f["fits"]["free"].get("c", 1.0), 1e-9) for f in fs]), 3),
                      fmt(_med([f["fits"]["fix"].get("a") for f in fs]), 1),
                      fmt(_med([f["fits"]["fix"].get("r2") for f in fs]), 3),
                      fmt(_med([f["diag"]["power"].get("p") for f in fs]), 3) + " +- "
                      + fmt(_med([f["diag"]["power"].get("p_se") for f in fs]), 3)))
    out.append("")


def table_hst(d: Dict[str, Any], out: List[str]) -> None:
    out.append("### B-HST：权重效率损失 E（T1 / T2）与电平比\n")
    out.append("| 场景 | 帧 | 方法 | E(T1) | E(T2) | 电平比(T1) | 源区 SNR p05(T1) |")
    out.append("|---|---|---|---|---|---|---|")
    for sc in d["scenarios"]:
        for fr in sc["frames"]:
            for m in ("phys", "interp_spline", "frame_scalar", "naive_pixel", "phys_unsep"):
                e = fr["methods"].get(m)
                if not e:
                    continue
                out.append("| %s | %d | %s | %s | %s | %s | %s |"
                           % (sc["scenario"], fr["frame_index"], m,
                              fmt(e["vs_T1_var_bg"].get("eff_loss")),
                              fmt(e["vs_T2_var_local"].get("eff_loss")),
                              fmt(e["vs_T1_var_bg"].get("level_ratio"), 4),
                              fmt(e["vs_T1_var_bg"].get("src_snr_p05"), 3)))
    out.append("")
    out.append("### B-HST：跨帧一致性（控制点 sigma 比 / 真值比，中位）\n")
    out.append("| 场景 | 比较 | median ratio | p95 绝对偏差 | n |")
    out.append("|---|---|---|---|---|")
    for sc in d["scenarios"]:
        for k, v in sc["cross_frame"].items():
            out.append("| %s | %s | %s | %s | %d |"
                       % (sc["scenario"], k, fmt(v["median_ratio"], 4),
                          fmt(v["p95_abs_dev"], 4), v["n"]))
    out.append("")


def table_real(d: Dict[str, Any], out: List[str]) -> None:
    out.append("### C-真实数据：一致性检验（无真值）\n")
    out.append("| 帧 | 自由斜率 c | 固定斜率 a | lever_var | 模型/奇族 hold-out | Pearson(log10) | R1/奇族 hold-out | 模型/二阶差分 |")
    out.append("|---|---|---|---|---|---|---|---|")
    for f in d["frames"]:
        c1 = f["C1_model_vs_holdout"].get("holdout_fam1", {})
        c2 = f["C1_model_vs_holdout"].get("second_diff", {})
        b1 = f["C1b_R1_vs_holdout"].get("holdout_fam1", {})
        out.append("| %s | %s | %s | %s | %s | %s | %s | %s |"
                   % (f["tag"], fmt(f["fit_family0"].get("c"), 4),
                      fmt(f["fit_family0"].get("a"), 1), fmt(f["diag"]["lever_var"], 3),
                      fmt(c1.get("median_ratio"), 4), fmt(c1.get("pearson_log10"), 3),
                      fmt(b1.get("median_ratio"), 4), fmt(c2.get("median_ratio"), 4)))
    out.append("")
    out.append("### C-真实数据：帧级标量 vs 区域 sigma（EXP-03 机理复现）\n")
    out.append("| 帧 | 帧级标量 | 区域 p05 | 区域 p95 | 区域离散度 | 模型场离散度 | 帧级 vs 区域最大偏差 |")
    out.append("|---|---|---|---|---|---|---|")
    for f in d["frames"]:
        c = f["C3"]
        out.append("| %s | %s | %s | %s | %s | %s | %s |"
                   % (f["tag"], fmt(c["sigma_frame_scalar"], 3), fmt(c["sigma_region_p05"], 3),
                      fmt(c["sigma_region_p95"], 3), fmt(c["region_dispersion"], 3),
                      fmt(c["model_dispersion"], 3), fmt(c["frame_vs_region_max_dev"], 3)))
    out.append("")


def table_gates(d: Dict[str, Any], out: List[str]) -> None:
    out.append("### 门与故障注入（ALL_PASS = %s）\n" % d.get("all_pass"))
    out.append("| 判定 | 门 | 预期 | 实测 |")
    out.append("|---|---|---|---|")
    for g in d["gates"]:
        out.append("| %s | %s | %s | %s |"
                   % (g["verdict"], g["gate"], g.get("expect", ""), g["detail"]))
    out.append("")


def _agg(runs: List[Dict[str, Any]], method: str, target: str, field: str) -> float:
    return _med([r["methods"].get(method, {}).get(target, {}).get(field) for r in runs])


def table_scope(d: Dict[str, Any], out: List[str]) -> None:
    T1, T2 = "vs_T1_var_bg", "vs_T2_var_local"
    for target, tn in ((T1, "T1 = var_bg（空背景口径）"), (T2, "T2 = var_local（逐像素总方差口径）")):
        out.append("### Q1 真值 sigma^2 场的空间方差被各方法解释的比例（R2_var，%s）\n" % tn)
        out.append("| 场景 | phys | interp_spline | phys_resid | frame_scalar | E_phys | E_interp |")
        out.append("|---|---|---|---|---|---|---|")
        names: List[str] = []
        for r in d["Q1_variance_explained"]:
            if r["name"] not in names:
                names.append(r["name"])
        for nm in names:
            runs = [r for r in d["Q1_variance_explained"] if r["name"] == nm]
            out.append("| %s | %s | %s | %s | %s | %s | %s |"
                       % (nm,
                          fmt(_agg(runs, "phys", target, "r2_var"), 4),
                          fmt(_agg(runs, "interp_spline", target, "r2_var"), 4),
                          fmt(_agg(runs, "phys_resid", target, "r2_var"), 4),
                          fmt(_agg(runs, "frame_scalar", target, "r2_var"), 4),
                          fmt(_agg(runs, "phys", target, "eff_loss")),
                          fmt(_agg(runs, "interp_spline", target, "eff_loss"))))
        out.append("")
        out.append("### Q2 弥散分量尺度扫描（%s，E 中位）\n" % tn)
        out.append("| ell_B [px] | ell/Delta | E_phys | E_interp | E_phys_resid | E_frame | E_dense |")
        out.append("|---|---|---|---|---|---|---|")
        for r in d["Q2_scale_sweep"]:
            runs = r["runs"]
            out.append("| %.0f | %.2f | %s | %s | %s | %s | %s |"
                       % (r["ell_B_px"], r["ell_over_delta"],
                          fmt(_agg(runs, "phys", target, "eff_loss")),
                          fmt(_agg(runs, "interp_spline", target, "eff_loss")),
                          fmt(_agg(runs, "phys_resid", target, "eff_loss")),
                          fmt(_agg(runs, "frame_scalar", target, "eff_loss")),
                          fmt(_agg(runs, "dense_patch", target, "eff_loss"))))
        out.append("")
        out.append("### Q3 亮度解释不了的噪声结构（%s）\n" % tn)
        out.append("| 场景 | E_phys | E_interp | E_phys_resid | E_frame | R2var phys | R2var interp | R2var phys_resid |")
        out.append("|---|---|---|---|---|---|---|---|")
        for r in d["Q3_detector_structure"]:
            runs = r["runs"]
            out.append("| %s | %s | %s | %s | %s | %s | %s | %s |"
                       % (r["name"],
                          fmt(_agg(runs, "phys", target, "eff_loss")),
                          fmt(_agg(runs, "interp_spline", target, "eff_loss")),
                          fmt(_agg(runs, "phys_resid", target, "eff_loss")),
                          fmt(_agg(runs, "frame_scalar", target, "eff_loss")),
                          fmt(_agg(runs, "phys", target, "r2_var"), 4),
                          fmt(_agg(runs, "interp_spline", target, "r2_var"), 4),
                          fmt(_agg(runs, "phys_resid", target, "r2_var"), 4)))
        out.append("")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "EXP06_TABLES.md"))
    a = ap.parse_args()
    R = X.RESULTS
    out: List[str] = ["# EXP-06 表格（由 results/exp06_*.json 生成）", ""]
    e1 = _load(R / "exp06_e1_analytic.json")
    if e1:
        out += ["## A 纯解析代数合成（臂 A）", ""]
        out.append("规模：%d 帧（%d 场景 x %d seed），%s，Delta=%d px。\n"
                   % (len(e1["frames"]), len({f["scene"] for f in e1["frames"]}),
                      e1["meta"]["n_seed"], e1["meta"]["shape"], e1["meta"]["delta_px"]))
        table_arm_a(e1, out)
        out.append("### A-负例（真值无效应）\n")
        out.append("| 负例 | seed | 结果 |")
        out.append("|---|---|---|")
        for n in e1["negatives"]:
            if n["name"] == "N1_flat_truth":
                out.append("| N1 平坦真值场 | %d | phys 离散度 %s；interp 离散度 %s；phys E=%s |"
                           % (n["seed"], fmt(n["dispersion"].get("phys"), 4),
                              fmt(n["dispersion"].get("interp_spline"), 4),
                              fmt(n["eff_loss_T2"].get("phys"), 8)))
            else:
                out.append("| N2 零噪声真值 | %d | truth_is_zero=%s；度量判退化=%s |"
                           % (n["seed"], n["truth_is_zero"], n["metrics_degenerated_flag"]
                              if "metrics_degenerated_flag" in n else n["metrics_degenerate_flag"]))
        out.append("")
    e2 = _load(R / "exp06_e2_hst.json")
    if e2:
        out += ["## B HST 真实模板 + 物理前向仿真（臂 B）", ""]
        table_hst(e2, out)
    e3 = _load(R / "exp06_e3_real.json")
    if e3:
        out += ["## C testdata 真实数据（臂 C）", ""]
        table_real(e3, out)
    e4 = _load(R / "exp06_e4_gates.json")
    if e4:
        out += ["## 门与故障注入", ""]
        table_gates(e4, out)
    e5 = _load(R / "exp06_e5_scope.json")
    if e5:
        out += ["## 作用域图谱（物理建模 vs 插值）", ""]
        table_scope(e5, out)
    Path(a.out).write_text("\n".join(out), encoding="utf-8")
    print("wrote", a.out, "lines", len(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
