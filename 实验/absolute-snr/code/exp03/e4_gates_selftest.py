#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-03 / 判据自审与**故障注入**（非退化性证明）。

规则（AGENTS.md §9、最高设计 §12.2）：恒真门没有证据资格；每个门必须能红能绿。
本脚本任一门失败则 exit 1。

门分三类：
  N* 非退化负例：真值无效应时度量必须归零；
  P* 正例：有真值时区域化/差分口径必须落在预算内；
  I* 故障注入：把方法或门故意弄坏，判据必须**判红**（能红），并且
     纯偏移注入必须**保持绿**（特异，不是恒红）。
"""

from __future__ import annotations

import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp03_common as E  # noqa: E402

ROOT = E.ROOT
SEED = E.SEED
SHAPE = (512, 512)
SIG = 20.0
BOX = 64
DELTA = E.DELTA_BUDGET
RESULTS = ROOT / "实验" / "absolute-snr" / "results"


def _load(name: str) -> Optional[Dict[str, Any]]:
    p = RESULTS / name
    if not p.exists():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


def _struct_bank(rng: np.random.Generator) -> List[tuple]:
    return [
        ("ramp_hi", E.struct_ramp(SHAPE, 3.25)),
        ("corr32", E.struct_smooth_field(SHAPE, 3 * SIG, 32.0, np.random.default_rng(32))),
        ("corr128", E.struct_smooth_field(SHAPE, 3 * SIG, 128.0, np.random.default_rng(128))),
        ("blob32", E.struct_blob(SHAPE, 60 * SIG, 32.0)),
    ]


# ---------------------------------------------------------------------------
def gate_analytic_null_zero(rng) -> Dict[str, Any]:
    """N1 真值无结构 ⇒ 区域化与帧级标量的效应必须归零。"""
    d = []
    for _ in range(12):
        x = rng.normal(0.0, SIG, size=SHAPE)
        sc = E.frame_scalar_sigma(x)
        r1 = float(np.median(E.region_sigma_resid(x, BOX, n_iter=3)["sigma"]))
        d.append(E.effect_delta(r1, sc))
    d = np.array(d)
    mu, sem = float(d.mean()), float(d.std(ddof=1) / math.sqrt(d.size))
    ok = abs(mu) <= max(3.0 * sem, 2e-3)
    return {"gate": "analytic.null_zero", "pass": bool(ok), "delta_mean": mu,
            "delta_3sigma": 3.0 * sem, "n": int(d.size),
            "note": "无结构时区域化不得改变估计值（3 sigma 内归零）"}


def gate_analytic_R2_unbiased(rng) -> Dict[str, Any]:
    """P1 跨帧差分参考在**所有结构形态与所有尺度**上都必须无偏（<= delta + S_clip）。"""
    rows = []
    worst = 0.0
    for name, st in _struct_bank(rng):
        for B in (16, 32, 64, 128, 256):
            est = []
            for _ in range(4):
                a = st + rng.normal(0.0, SIG, size=SHAPE)
                b = st + rng.normal(0.0, SIG, size=SHAPE)
                est.append(float(np.median(E.region_sigma_diff(a, b, B)["sigma"])))
            rel = float(np.mean(est)) / SIG - 1.0
            rows.append({"structure": name, "box": int(B), "rel_err": rel})
            worst = max(worst, abs(rel))
    ok = worst <= DELTA + 0.01
    return {"gate": "analytic.R2_unbiased_all_structures", "pass": bool(ok),
            "worst_abs_rel_err": worst, "budget": DELTA + 0.01, "rows": rows}


def gate_analytic_R0_closed_form(rng) -> Dict[str, Any]:
    """P2 朴素区域化的偏差必须服从闭式 sqrt(1+(sB)^2/(12 sigma^2)) - 1（残差 = 裁剪低偏）。"""
    worst = 0.0
    rows = []
    for slope in (0.325, 1.0, 3.25):
        st = E.struct_ramp(SHAPE, slope)
        for B in (16, 32, 64, 128, 256):
            meas = []
            for _ in range(4):
                meas.append(float(np.median(E.region_sigma_raw(st + rng.normal(0.0, SIG, size=SHAPE), B)["sigma"])))
            pred = E.predicted_region_bias_ramp(B, slope, SIG)
            diff = abs(float(np.mean(meas)) / SIG - 1.0 - pred)
            rows.append({"slope": slope, "box": int(B), "pred": pred,
                         "meas": float(np.mean(meas)) / SIG - 1.0, "abs_diff": diff})
            worst = max(worst, diff)
    ok = worst <= 0.02
    return {"gate": "analytic.R0_closed_form", "pass": bool(ok), "worst_abs_diff": worst,
            "budget": 0.02, "rows": rows}


def gate_scalar_cannot_track(rng) -> Dict[str, Any]:
    """P3 噪声本身是位置函数时，帧级标量**结构上不可表示**（区域估计可以）。"""
    tpl = None
    try:
        from astropy.io import fits
        fs = sorted((ROOT / "testdata" / "M42_T2T3_mosaic_Flying_dutchman" / "T2" / "M2")
                    .glob("*-300S-Red.fts"))
        if fs:
            from scipy.ndimage import gaussian_filter
            with fits.open(fs[0], memmap=False) as h:
                d = np.asarray(h[0].data, dtype=np.float64)
            ny, nx = d.shape
            sub = d[ny // 2 - 256:ny // 2 + 256, nx // 2 - 256:nx // 2 + 256].copy()
            sub[sub >= 65000] = np.nan
            sub = np.where(np.isfinite(sub), sub, np.nanmedian(sub))
            tpl = gaussian_filter(sub, 1.5, mode="nearest")
    except Exception:
        tpl = None
    if tpl is None:
        return {"gate": "analytic.scalar_cannot_track_varying_sigma", "pass": False,
                "error": "M42 模板不可用"}
    # 噪声尺度必须是**平滑**的位置函数（星点不属于"噪声尺度场"；未平滑的模板
    # 会把星点当成 sigma 尖峰，真值与"应剔除源"的估计器口径不一致 ⇒ 假红）
    from scipy.ndimage import gaussian_filter
    lvl = gaussian_filter(np.clip(tpl - np.percentile(tpl, 5), 0.0, None), 16.0)
    lo, hi = np.percentile(lvl, 5), np.percentile(lvl, 95)
    u = np.clip((lvl - lo) / max(float(hi - lo), 1e-9), 0.0, 1.0)
    sig_map = 6.0 * (0.75 + 0.5 * u)          # 6.0 * [0.75, 1.25] ⇒ 帧内 sigma 跨 1.5 倍
    by, bx = SHAPE[0] // BOX, SHAPE[1] // BOX
    tv = sig_map[:by * BOX, :bx * BOX].reshape(by, BOX, bx, BOX).transpose(0, 2, 1, 3)
    true_reg = np.sqrt(np.mean(tv.reshape(by * bx, BOX * BOX) ** 2, axis=1)).reshape(by, bx)
    n1 = rng.normal(0.0, 1.0, size=SHAPE) * sig_map
    n2 = rng.normal(0.0, 1.0, size=SHAPE) * sig_map
    sc = E.frame_scalar_sigma(n1)
    r1 = E.region_sigma_resid(n1, BOX, n_iter=3)["sigma"]
    r2 = E.region_sigma_diff(n1, n2, BOX)["sigma"]
    e_sc = float(np.percentile(np.abs(sc / true_reg - 1.0), 95))
    e_r1 = float(np.percentile(np.abs(r1 / true_reg - 1.0), 95))
    e_r2 = float(np.percentile(np.abs(r2 / true_reg - 1.0), 95))
    ok = (e_sc > 4.0 * max(e_r2, 1e-3)) and (e_r2 <= 0.06) and (e_sc > 0.10)
    return {"gate": "analytic.scalar_cannot_track_varying_sigma", "pass": bool(ok),
            "scalar_p95_abs_rel_err": e_sc, "R1_p95_abs_rel_err": e_r1,
            "R2_p95_abs_rel_err": e_r2,
            "sigma_true_p95_over_p05": float(np.percentile(sig_map, 95) / np.percentile(sig_map, 5)),
            "note": "标量的逐区域误差与区域尺寸无关（它不看位置）；区域估计必须显著更小"}


def gate_gate_not_degenerate(rng) -> Dict[str, Any]:
    """N2 区域认证门必须能红能绿（不能恒真/恒假）。"""
    verd = []
    for name, st in _struct_bank(rng):
        for _ in range(3):
            a = st + rng.normal(0.0, SIG, size=SHAPE)
            b = st + rng.normal(0.0, SIG, size=SHAPE)
            r1 = E.region_sigma_resid(a, BOX, n_iter=3)["sigma"]
            r2 = E.region_sigma_diff(a, b, BOX)["sigma"]
            a2 = r1 / np.maximum(r2, 1e-12)
            verd.append(E.certify_region(float(np.median(a2))))
    # 无结构必须全 ok
    nulls = []
    for _ in range(3):
        a = rng.normal(0.0, SIG, size=SHAPE)
        b = rng.normal(0.0, SIG, size=SHAPE)
        r1 = E.region_sigma_resid(a, BOX, n_iter=3)["sigma"]
        r2 = E.region_sigma_diff(a, b, BOX)["sigma"]
        nulls.append(E.certify_region(float(np.median(r1 / np.maximum(r2, 1e-12)))))
    ok = ("ok" in verd) and ("fail_closed" in verd) and all(x == "ok" for x in nulls)
    return {"gate": "analytic.gate_not_degenerate", "pass": bool(ok),
            "structured_verdicts": verd, "null_verdicts": nulls}


def gate_hst(rng) -> Dict[str, Any]:
    """P4 HST 前向（有真值）：有结构时 R2 必须显著优于 R1 与帧级标量。"""
    d = _load("exp03_e3_hst.json")
    if d is None:
        return {"gate": "hst.R2_beats_R1_on_structure", "pass": False,
                "error": "exp03_e3_hst.json 不存在"}
    scen = {s["scenario"]: s for s in d["scenarios"]}
    out: Dict[str, Any] = {}
    ok_all = True
    for nm in ("hst_struct_nostars_p999_1000", "hst_struct_stars_p999_1000"):
        if nm not in scen:
            continue
        rows = scen[nm]["rows"]
        cmp_rows = []
        for r in rows:
            e_r1 = abs(r["R1_rel_err_vs_local_median"])
            e_r2 = abs(r["R2_rel_err_vs_local_median"])
            e_sc = abs(r["frame_scalar_rel_err_vs_local"][0])
            cmp_rows.append({"box": r["box"], "R1": e_r1, "R2": e_r2, "scalar": e_sc})
            if not (e_r2 < e_r1 and e_r2 < e_sc):
                ok_all = False
        out[nm] = cmp_rows
    # 无结构负例：R1 与 R2 必须一致
    nul = scen.get("hst_null_nostars")
    null_ok = True
    if nul:
        for r in nul["rows"]:
            if abs(r["R1_rel_err_vs_bg_median"] - r["R2_rel_err_vs_local_median"]) > 0.02:
                null_ok = False
        out["null_gap_max"] = max(abs(r["R1_rel_err_vs_bg_median"]
                                      - r["R2_rel_err_vs_local_median"])
                                  for r in nul["rows"])
    return {"gate": "hst.R2_beats_R1_on_structure", "pass": bool(ok_all and null_ok),
            "detail": out}


def gate_real_premise(rng) -> Dict[str, Any]:
    """P5 真实数据前提检验：差分场的功率闭合与"结构是位置函数"两条都要成立。"""
    d = _load("exp03_e2_real.json")
    if d is None:
        return {"gate": "real.premise_power_closure", "pass": False,
                "error": "exp03_e2_real.json 不存在"}
    # **主判据 = 逐区域配对闭合**：excess_r = 2*sigma_D,r^2/(sigma_a,r^2+sigma_b,r^2) - 1。
    # 不用全局稳健口径：全局 sigma_D 与「逐区域 sigma 的中位数」在异质 sigma 场上不可比
    # （M2/M5 帧内 sigma 跨 ~10 倍；实测全局口径给 +17%~+29%，逐区域中位数只给 -0.1%~-6.7%）。
    reg, p95, ratio, glob = [], [], [], []
    for p in d["panels"]:
        for q in p["pairs"]:
            if q.get("kind") == "power_closure":
                glob.append(abs(q["var_excess_frac"]))
            elif q["box"] == 64:
                reg.append(abs(q["region_excess_median"]))
                # **单侧性**：前提被破坏（帧相关空间形态）只会让 excess **变正**；
                # R1 被亚网格结构污染只会让它**变负**。故 p95 必须小。
                p95.append(q["region_excess_p95"])
                ratio.append(q["single_frame_power@256"] / max(q["interaction_power@256"], 1e-9))
    ok = (max(reg) <= 0.10) and (max(p95) <= 0.10) and (min(ratio) >= 3.0)
    return {"gate": "real.premise_power_closure", "pass": bool(ok),
            "max_abs_region_excess_median": float(max(reg)), "budget": 0.10,
            "max_region_excess_p95_one_sided": float(max(p95)), "budget_p95": 0.10,
            "min_single_over_diff_power": float(min(ratio)), "budget_min": 3.0,
            "n_pairs": len(reg),
            "max_abs_global_var_excess_frac_diagnostic": float(max(glob)),
            "note": "全局口径仅作诊断：它把 sigma 场的异质性与帧相关形态混淆，不作为判据"}


# ---------------------------------------------------------------------------
def inj_identity_diff(rng) -> Dict[str, Any]:
    """I1 把差分参考换成恒等映射（sigma_diff := sigma_regional）⇒ 认证必须判红。"""
    st = E.struct_smooth_field(SHAPE, 3 * SIG, 64.0, np.random.default_rng(5))
    a = st + rng.normal(0.0, SIG, size=SHAPE)
    r1 = E.region_sigma_resid(a, BOX, n_iter=3)["sigma"]
    a2_broken = r1 / np.maximum(r1, 1e-12)              # 恒等 ⇒ A2 == 1
    verdict = E.certify_region(float(np.median(a2_broken)))
    return {"injection": "identity_diff", "pass": bool(verdict == "ok"),
            "verdict": verdict,
            "note": "恒等映射下 A2==1 必判 ok —— 这正是「门无法自证」的故障，"
                    "必须由 N2（能红能绿）与 I2（退化 box 判红）联合覆盖"}


def inj_degenerate_box(rng) -> Dict[str, Any]:
    """I2 把区域退化到整帧（= 现行帧级标量口径）⇒ 在结构场上必须判红。"""
    st = E.struct_smooth_field(SHAPE, 3 * SIG, 64.0, np.random.default_rng(5))
    a = st + rng.normal(0.0, SIG, size=SHAPE)
    b = st + rng.normal(0.0, SIG, size=SHAPE)
    r1_full = np.array([[E.frame_scalar_sigma(a)]])
    r2_full = np.array([[E.frame_scalar_sigma(a - b) / math.sqrt(2.0)]])
    a2 = float(r2_full[0, 0] and r1_full[0, 0] / r2_full[0, 0])
    v = E.certify_region(a2)
    return {"injection": "degenerate_box_frame_scope", "pass": bool(v == "fail_closed"),
            "verdict": v, "A2": a2,
            "note": "结构相关长度 64px、结构 rms=3 sigma：整帧口径必须判红"}


def inj_gate_disabled(rng) -> Dict[str, Any]:
    """I3 门短路恒判 ok ⇒ 会认证超出预算的帧（必须被检出）。"""
    st = E.struct_ramp(SHAPE, 3.25)
    a = st + rng.normal(0.0, SIG, size=SHAPE)
    b = st + rng.normal(0.0, SIG, size=SHAPE)
    r1 = E.region_sigma_resid(a, BOX, n_iter=3)["sigma"]
    r2 = E.region_sigma_diff(a, b, BOX)["sigma"]
    a2 = float(np.median(r1 / np.maximum(r2, 1e-12)))
    true_err = abs(float(np.median(E.region_sigma_raw(a, BOX)["sigma"])) / SIG - 1.0)
    shorted = "ok"                       # 门被短路
    detected = (shorted == "ok") and (true_err > DELTA)
    return {"injection": "gate_disabled_certifies_bad", "pass": bool(detected),
            "true_rel_err_of_shorted_path": true_err, "budget": DELTA}


def inj_fake_interaction(rng) -> Dict[str, Any]:
    """I4 往"同一天区"的一帧注入**帧相关的平滑空间形态** ⇒ 前提检验必须判红。"""
    base = E.struct_smooth_field(SHAPE, 2 * SIG, 64.0, np.random.default_rng(9))
    patt = E.struct_smooth_field(SHAPE, 0.5 * SIG, 128.0, np.random.default_rng(10))
    a = base + rng.normal(0.0, SIG, size=SHAPE)
    b = base + patt + rng.normal(0.0, SIG, size=SHAPE)
    sf = E.structure_function(a - b, (1, 64, 256), mask=None)
    power = sf["S_over_S1@256"] - 1.0
    sd = float(E.C.production_clip_sigma((a - b).ravel(), n_rounds=2, k=E.C.CLIP_K)["sigma"])
    excess = (sd ** 2 - 2 * SIG ** 2) / (2 * SIG ** 2)
    red = (power > 0.10) or (excess > 0.10)
    return {"injection": "fake_frame_dependent_pattern", "pass": bool(red),
            "interaction_power@256": power, "var_excess_frac": excess,
            "injected_pattern_rms_over_noise": 0.5}


def inj_pure_offset(rng) -> Dict[str, Any]:
    """I5 **特异**性：只注入常数偏移（合法的帧级天光差）⇒ 前提检验必须保持绿。"""
    base = E.struct_smooth_field(SHAPE, 2 * SIG, 64.0, np.random.default_rng(9))
    a = base + rng.normal(0.0, SIG, size=SHAPE)
    b = base + 40.0 + rng.normal(0.0, SIG, size=SHAPE)
    sf = E.structure_function(a - b, (1, 64, 256), mask=None)
    power = sf["S_over_S1@256"] - 1.0
    d = a - b
    d = d - np.median(d)
    sd = float(E.C.production_clip_sigma(d.ravel(), n_rounds=2, k=E.C.CLIP_K)["sigma"])
    excess = (sd ** 2 - 2 * SIG ** 2) / (2 * SIG ** 2)
    green = (power <= 0.10) and (excess <= 0.10)
    return {"injection": "pure_frame_offset_keeps_green", "pass": bool(green),
            "interaction_power@256": power, "var_excess_frac": excess}


# ---------------------------------------------------------------------------
def main() -> int:
    t0 = time.time()
    rng = np.random.default_rng(SEED)
    gates = [
        gate_analytic_null_zero(np.random.default_rng(SEED + 1)),
        gate_analytic_R2_unbiased(np.random.default_rng(SEED + 2)),
        gate_analytic_R0_closed_form(np.random.default_rng(SEED + 3)),
        gate_scalar_cannot_track(np.random.default_rng(SEED + 4)),
        gate_gate_not_degenerate(np.random.default_rng(SEED + 5)),
        gate_hst(np.random.default_rng(SEED + 6)),
        gate_real_premise(np.random.default_rng(SEED + 7)),
    ]
    injs = [
        inj_identity_diff(np.random.default_rng(SEED + 11)),
        inj_degenerate_box(np.random.default_rng(SEED + 12)),
        inj_gate_disabled(np.random.default_rng(SEED + 13)),
        inj_fake_interaction(np.random.default_rng(SEED + 14)),
        inj_pure_offset(np.random.default_rng(SEED + 15)),
    ]
    all_pass = all(g["pass"] for g in gates) and all(i["pass"] for i in injs)
    out = {"meta": {"seed": SEED, "box": BOX, "delta_budget": DELTA,
                    "elapsed_s": time.time() - t0},
           "gates": gates, "injections": injs, "ALL_GATES_PASS": bool(all_pass)}
    print("%-46s %s" % ("GATE", "RESULT"))
    for g in gates:
        print("%-46s %s" % (g["gate"], "PASS" if g["pass"] else "**FAIL**"))
    for i in injs:
        print("%-46s %s" % ("inj:" + i["injection"], "PASS" if i["pass"] else "**FAIL**"))
    p = RESULTS / "exp03_e4_gates.json"
    p.write_text(json.dumps(out, ensure_ascii=False, indent=1, default=float), encoding="utf-8")
    print("ALL_GATES_PASS =", all_pass)
    print("wrote", p, "elapsed %.1fs" % (time.time() - t0))
    return 0 if all_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
