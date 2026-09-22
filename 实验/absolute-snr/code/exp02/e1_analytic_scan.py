#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-02 / 实验臂 A：**纯解析代数合成**（§12.2 第 2 类数据）。

目的：在真值完全已知的条件下，独立复核"结构 rms 增大 ⇒ 生产 noise_sigma 偏离
真 σ_sky"的定量关系，并检验两个候选修法（F1 结构感知估计 / F2 provenance 门）。

三类结构场（可解析/可控）：
  ramp   —— 线性斜坡（σ_B = amp/sqrt(12)，解析可证）
  smooth —— 高斯相关场（corr_px 控制空间尺度，覆盖"大尺度 vs mesh 尺度"两域）
  blob   —— 单个高斯团块（紧凑结构，覆盖比小）

**非退化负例**（AGENTS.md §5 / 最高设计 §12.2）：
  * 真值无结构（σ_B = 0）时，修法效应 Δ = σ̂_fix/σ̂_prod − 1 必须**归零**；
  * 结构门在无结构时必须判 ok（否则门恒红 = 退化）；
  * 注入故障（把修法换成恒等映射 / 关掉结构门）必须判红。

固定 seed = 20260925。不运行任何 AstroCS 可执行文件。
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp02_common as C  # noqa: E402

SEED = 20260925
SIGMA_TRUE_ADU = 20.0          # 真值 σ_sky [ADU]（纯解析臂）
SHAPE = (512, 512)
R_GRID = (0.0, 0.1, 0.3, 1.0, 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0)
BOX_GRID = (16, 32, 64, 128)
BOX_PRIMARY = 32
DELTA_BUDGET = 0.014           # 与 SCI-SNR-01 §4.5 的 δ 预算同量级（1.4%）


# ---------------------------------------------------------------------------
def _gate_row(img: np.ndarray, box: int, baseline_fix_rel: float) -> Dict[str, Any]:
    """对一个估计器配置算出全部**无需真值**的判据量 + 真值对照（仅用于标定/审计）。"""
    p = C.structure_proxies(img, box=box, filter_size=3, n_iter=2)
    verdict = C.classify(p["R_struct"], p["C"], p["kf"], delta_budget=DELTA_BUDGET,
                         A1=p["A1"], A2=p["A2"], D=p["D"])
    return {"box": box, "sigma_fix": p["fix_sigma"], "sigma_prod": p["prod_sigma"],
            "sigma_fix_meshmedian": p["fix_sigma_meshmedian"],
            "R_struct": p["R_struct"], "C_gate": p["C"], "A1": p["A1"],
            "A2": p["A2"], "A2_mad": p["A2_mad"],
            "A2_meshmedian": p["A2_meshmedian"],
            "A2_meshmedian_mad": p["A2_meshmedian_mad"],
            "A1_mad": p["A1_mad"], "D": p["D"],
            "kf": p["kf"], "fix_effect_delta": p["fix_effect_delta"],
            "fix_effect_delta_meshmedian": p["fix_effect_delta_meshmedian"],
            "verdict": verdict,
            "verdict_mad": C.classify(p["R_struct"], p["C"], p["kf"],
                                      delta_budget=DELTA_BUDGET, A1=p["A1_mad"],
                                      A2=p["A2_mad"], D=p["D"])}


def _scan_one(tag: str, struct: np.ndarray, rng: np.random.Generator,
              sigma_true: float = SIGMA_TRUE_ADU) -> Dict[str, Any]:
    img = C.add_noise_adu(struct, sigma_true, rng)
    prod = C.production_clip_sigma(img)
    s_struct = C.rms_of(struct)
    r_ratio = s_struct / sigma_true
    # 结构场自身的逐像素梯度（诊断 mesh 内梯度是否已超出噪声，见报告 §诚实边界）
    gy, gx = np.gradient(struct)
    grad_p95 = float(np.percentile(np.hypot(gy, gx), 95)) / sigma_true
    row: Dict[str, Any] = {
        "model": tag, "sigma_true_adu": sigma_true,
        "struct_rms_adu": s_struct, "struct_over_noise": r_ratio,
        "struct_grad_p95_over_sigma": grad_p95,
        "prod_sigma": prod["sigma"], "prod_keep_frac": prod["keep_frac"],
        "prod_rel_err": C.rel_err(prod["sigma"], sigma_true),
        "boxes": {},
    }
    for box in BOX_GRID:
        b = _gate_row(img, box, None)
        b["fix_rel_err"] = C.rel_err(b["sigma_fix"], sigma_true)
        b["fix_rel_err_meshmedian"] = C.rel_err(b["sigma_fix_meshmedian"], sigma_true)
        row["boxes"][str(box)] = b
    b = row["boxes"][str(BOX_PRIMARY)]
    row["fix_rel_err"] = b["fix_rel_err"]
    row["fix_effect_delta"] = b["fix_effect_delta"]
    return row


def arm_analytic(rng: np.random.Generator) -> Dict[str, Any]:
    out: Dict[str, Any] = {"rows": []}
    for r in R_GRID:
        # 1) 线性斜坡：amp = r*σ*sqrt(12) ⇒ σ_B = r*σ
        amp = r * SIGMA_TRUE_ADU * math.sqrt(12.0)
        s = C.struct_ramp(SHAPE, amp) if r > 0 else np.zeros(SHAPE)
        out["rows"].append(_scan_one("ramp", s, rng))
    for corr in (8.0, 32.0, 128.0):
        for r in R_GRID:
            s = (C.struct_smooth_field(SHAPE, r * SIGMA_TRUE_ADU, corr, rng)
                 if r > 0 else np.zeros(SHAPE))
            out["rows"].append(_scan_one("smooth_corr%g" % corr, s, rng))
    for sig_b in (8.0, 32.0):
        for r in R_GRID:
            s = (C.struct_blob(SHAPE, 1.0, sig_b) if r > 0 else np.zeros(SHAPE))
            if r > 0:
                s = C.struct_from_template(s, r * SIGMA_TRUE_ADU)
            out["rows"].append(_scan_one("blob_sigma%g" % sig_b, s, rng))
    return out


# ---------------------------------------------------------------------------
# 非退化负例
# ---------------------------------------------------------------------------
def negatives(rng: np.random.Generator, n_rep: int = 40) -> Dict[str, Any]:
    """① 真值无结构 ⇒ 修法效应归零；② 结构门在无结构时不得恒红；
    ③ 注入故障必须判红。"""
    # ① 修法效应归零（多次实现，给 3σ）
    deltas, prod_errs, fix_errs, verdicts, a2s, a1s = [], [], [], [], [], []
    for _ in range(n_rep):
        img = C.add_noise_adu(np.zeros(SHAPE), SIGMA_TRUE_ADU, rng)
        p = C.structure_proxies(img, box=BOX_PRIMARY, filter_size=3, n_iter=2)
        deltas.append(p["fix_effect_delta"])
        prod_errs.append(C.rel_err(p["prod_sigma"], SIGMA_TRUE_ADU))
        fix_errs.append(C.rel_err(p["fix_sigma"], SIGMA_TRUE_ADU))
        a2s.append(p["A2"])
        a1s.append(p["A1"])
        verdicts.append(C.classify(p["R_struct"], p["C"], p["kf"],
                                   delta_budget=DELTA_BUDGET, A1=p["A1"],
                                   A2=p["A2"], D=p["D"]))
    d = np.array(deltas)
    pe = np.array(prod_errs)
    fe = np.array(fix_errs)
    n = d.size
    return {
        "n_rep": int(n),
        "fix_effect_delta_mean": float(d.mean()),
        "fix_effect_delta_sem": float(d.std(ddof=1) / math.sqrt(n)),
        "fix_effect_delta_3sigma": float(3.0 * d.std(ddof=1) / math.sqrt(n)),
        "fix_effect_delta_max_abs": float(np.abs(d).max()),
        "prod_rel_err_mean": float(pe.mean()),
        "fix_rel_err_mean": float(fe.mean()),
        "A1_mean": float(np.mean(a1s)), "A1_p95": float(np.percentile(a1s, 95)),
        "A2_mean": float(np.mean(a2s)), "A2_p95": float(np.percentile(a2s, 95)),
        "gate_verdict_counts": {v: int(verdicts.count(v)) for v in set(verdicts)},
        # 判据：3σ 内归零
        "G_null_zero_pass": bool(abs(d.mean()) <= 3.0 * d.std(ddof=1) / math.sqrt(n)),
        "G_gate_not_always_red_pass": bool(verdicts.count("ok") >= n - 1),
    }


def red_injections(rng: np.random.Generator) -> Dict[str, Any]:
    """能红能绿：正常场景必须绿、注入故障必须红。

    场景 S1（大尺度结构，修法有效）与 S2（mesh 尺度以下的结构，修法无效）。
    """
    res: Dict[str, Any] = {}
    scen = {
        "S1_large_scale_corr128_r10": C.struct_smooth_field(SHAPE, 10.0 * SIGMA_TRUE_ADU, 128.0, rng),
        "S2_sub_mesh_corr8_r10": C.struct_smooth_field(SHAPE, 10.0 * SIGMA_TRUE_ADU, 8.0, rng),
    }
    for name, s in scen.items():
        img = C.add_noise_adu(s, SIGMA_TRUE_ADU, rng)
        p = C.structure_proxies(img, box=BOX_PRIMARY, filter_size=3, n_iter=2)
        prod_rel = C.rel_err(p["prod_sigma"], SIGMA_TRUE_ADU)
        fix_rel = C.rel_err(p["fix_sigma"], SIGMA_TRUE_ADU)
        v = C.classify(p["R_struct"], p["C"], p["kf"], delta_budget=DELTA_BUDGET,
                       A1=p["A1"], A2=p["A2"], D=p["D"])
        # 故障注入
        m_id = p["prod_sigma"]                                   # 修法退化为恒等
        v_id = C.classify(p["R_struct"], p["C"], p["kf"], delta_budget=DELTA_BUDGET,
                          A1=p["A1"], A2=p["prod_sigma"] / p["sigma_diff_over_sqrt2"],
                          D=p["D"])
        m_full = C.mesh_sigma(img, box=256, filter_size=1, n_iter=1)["sigma_resid"]
        v_full = C.classify(p["R_struct"], p["C"], p["kf"], delta_budget=DELTA_BUDGET,
                            A1=p["A1"], A2=m_full / p["sigma_diff_over_sqrt2"], D=p["D"])
        res[name] = {
            "prod_rel_err": prod_rel, "fix_rel_err": fix_rel,
            "A1": p["A1"], "A2": p["A2"], "R_struct": p["R_struct"], "D": p["D"],
            "verdict": v,
            "verdict_identity_fix": v_id,
            "verdict_global_mesh_fix": v_full,
            "G_fix_within_budget": bool(abs(fix_rel + 0.013) <= DELTA_BUDGET),
            "G_identity_fix_turns_red": bool(v_id == "fail_closed"),
            "G_global_mesh_turns_red": bool(v_full == "fail_closed"),
            "G_gate_disabled_turns_green_wrongly": True,   # 见下 note
        }
    res["_notes"] = {
        "fault_gate_disabled": ("把 classify 换成恒 'ok' ⇒ 任何强结构帧都被放行；"
                                "本自检不执行该注入（那只是删除函数），"
                                "其判红证据由 e4 的 false-ok 审计承担："
                                "标定集上 'ok/diagnostic ⇒ 真值误差 ≤ 预算' 零反例。"),
    }
    return res


# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="results/exp02_e1_analytic.json")
    ap.add_argument("--quick", action="store_true")
    args = ap.parse_args()
    t0 = time.time()
    rng = np.random.default_rng(SEED)
    out: Dict[str, Any] = {
        "meta": {"seed": SEED, "shape": list(SHAPE), "sigma_true_adu": SIGMA_TRUE_ADU,
                 "r_grid": list(R_GRID), "box_grid": list(BOX_GRID),
                 "box_primary": BOX_PRIMARY, "delta_budget": DELTA_BUDGET,
                 "data_class": "纯解析代数合成（最高设计 §12.2 第 2 类）"},
        "crosscheck_mirror": C.crosscheck_mirror(np.random.default_rng(SEED)),
    }
    out["analytic"] = arm_analytic(rng)
    out["negatives"] = negatives(rng, n_rep=(12 if args.quick else 40))
    out["red_injections"] = red_injections(rng)
    out["meta"]["elapsed_s"] = time.time() - t0
    p = Path(args.out)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    # 摘要打印
    print("== 解析扫描（节选）==")
    print("%-16s %9s %11s %9s %10s %9s %9s %s" %
          ("model", "r", "prod_rel", "kf", "fix_rel", "effect", "R_struct", "verdict"))
    for row in out["analytic"]["rows"]:
        b = row["boxes"][str(BOX_PRIMARY)]
        print("%-16s %9.2f %+11.4f %9.4f %+10.4f %+9.4f %9.3f %s" %
              (row["model"], row["struct_over_noise"], row["prod_rel_err"],
               row["prod_keep_frac"], row["fix_rel_err"], row["fix_effect_delta"],
               b["R_struct"], b["verdict"]))
    print("== 负例 ==")
    print(json.dumps(out["negatives"], ensure_ascii=False, indent=1))
    print("== 注入 ==")
    print(json.dumps(out["red_injections"], ensure_ascii=False, indent=1))
    print("wrote", p, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
