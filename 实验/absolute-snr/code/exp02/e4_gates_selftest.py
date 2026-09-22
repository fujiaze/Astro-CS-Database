#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-02 / 判据自审（能红能绿）+ 候选修法比较。

读三臂结果 JSON，做四件**可执行**的事：
  A. **假绿审计**（能红）：在有真值的臂（解析 / HST）上要求
     "verdict ∈ {ok, diagnostic_only} ⇒ |修法相对误差 − 无结构基线| ≤ 预算"
     **零反例**；反例数 > 0 即判红。
  B. **非退化审计**（不恒红/不恒绿）：三档判据在三臂上的分布；无结构场景必须判 ok。
  C. **故障注入**（能红）：恒等修法 / 整帧 mesh / 关闭门 —— 必须判红。
  D. **候选比较**：F0 生产全局 / F1a mesh σ 中值（SExtractor backsig 口径）/
     F1b 残差全局裁剪 RMS / F2 provenance 门，给出定量对照与推荐。

用法：python3 code/exp02/e4_gates_selftest.py --results results
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp02_common as C  # noqa: E402

DELTA_BUDGET = 0.014
CERTIFIED = ("ok", "diagnostic_only")


def _load(p: Path) -> Optional[Dict[str, Any]]:
    return json.loads(p.read_text(encoding="utf-8")) if p.exists() else None


# ── 认证判据的口径（关键，勿混）────────────────────────────────────────────
# 门里的 δ=1.4% 是**结构项**预算：A2 = σ̂_fix/(σ_diff/sqrt2) 两侧用**同一个**裁剪 RMS
# 估计量，固有裁剪低偏在分子分母中相消（实测纯噪声 A2 = 0.9993~1.0019），
# 故 A2 ≤ 1+δ 是对"结构残留"的判据。
# 而 σ̂ 相对**真值**的总误差还含固有裁剪低偏 S_clip（EXP-01 测得 1.38%~1.43%，
# 已作为独立系统项入 S_sys 预算）⇒ 认证的**审计**判据是
#     |σ̂_fix/σ_true − 1| ≤ δ + S_clip(本臂实测无结构基线)
# 两个判据同时报出；只报 excess（相对无结构基线）会把"结构把估计拉回真值"的
# 情形误判为违规（HST 臂实测：A2=0.9975 时 excess=+2.06% 但 |误差|=0.47% < 基线 1.81%）。
def _cert_audit(rows_iter, baseline_of, delta_budget=DELTA_BUDGET) -> Dict[str, Any]:
    counts: Dict[str, int] = {}
    viol_total: List[Dict[str, Any]] = []
    viol_excess: List[Dict[str, Any]] = []
    n_cert = 0
    worst_total = 0.0
    worst_excess = 0.0
    for key, b in rows_iter:
        counts[b["verdict"]] = counts.get(b["verdict"], 0) + 1
        if b["verdict"] not in CERTIFIED:
            continue
        n_cert += 1
        base = baseline_of(key)
        total = abs(b["fix_rel_err"])
        excess = b["fix_rel_err"] - base
        budget_total = delta_budget + abs(base)
        worst_total = max(worst_total, total)
        worst_excess = max(worst_excess, abs(excess))
        rec = {"key": str(key), "fix_rel_err": b["fix_rel_err"], "base": base,
               "total": total, "excess": excess, "A2": b["A2"], "C": b["C_gate"],
               "D": b["D"]}
        if total > budget_total:
            viol_total.append(rec)
        if abs(excess) > delta_budget:
            viol_excess.append(rec)
    return {"verdict_counts": counts, "n_certified": n_cert,
            "n_false_certified_total_budget": len(viol_total),
            "n_false_certified_excess_budget": len(viol_excess),
            "worst_abs_total_err_among_certified": worst_total,
            "worst_abs_excess_among_certified": worst_excess,
            "violations_total_budget": viol_total[:10],
            "violations_excess_budget": viol_excess[:10],
            "G_no_false_certification_pass": len(viol_total) == 0}


def audit_analytic(d: Dict[str, Any]) -> Dict[str, Any]:
    rows = d["analytic"]["rows"]
    base = {r["model"]: r["fix_rel_err"] for r in rows if r["struct_over_noise"] == 0.0}
    it = (((r["model"]), b) for r in rows for box, b in r["boxes"].items())
    out = _cert_audit(it, lambda m: base[m])
    out["n_rows"] = sum(len(r["boxes"]) for r in rows)
    return out


def audit_hst(d: Dict[str, Any]) -> Dict[str, Any]:
    rows = d["rows"]
    # 基线：**同一 variant** 在 target_p999_e=0（无底图）时的修法相对误差
    base = {r["variant"]: r["fix_rel_err"] for r in rows if r["target_p999_e"] == 0.0}
    it = ((r["variant"], r) for r in rows)
    out = _cert_audit(it, lambda v: base[v])
    out["n_rows"] = len(rows)
    out["negatives"] = d["negatives_no_base"]
    return out


def audit_real(d: Dict[str, Any]) -> Dict[str, Any]:
    rows = d["frames"]
    counts: Dict[str, int] = {}
    for r in rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
    # 真实臂**没有真值**，因此"门是否恒红"不是缺陷证据：门恒红只说明"这些帧里没有
    # 一帧能在 1.4% 预算内被认证为 σ_sky"，这正是应报的结论。非退化性（不恒红/不恒绿）
    # 由**有真值的两臂**承担（解析臂 + HST 臂），真实臂只登记分布与量级。
    return {"n_frames": len(rows), "verdict_counts": counts,
            "prod_over_fix_p50": float(np.median([r["prod_sigma"] / r["fix_sigma"] for r in rows])),
            "prod_over_fix_max": float(np.max([r["prod_sigma"] / r["fix_sigma"] for r in rows])),
            "kf_min": float(np.min([r["prod_keep_frac"] for r in rows])),
            "kf_max": float(np.max([r["prod_keep_frac"] for r in rows])),
            "A2_p50": float(np.median([r["A2"] for r in rows])),
            "A2_min": float(np.min([r["A2"] for r in rows])),
            "A2_max": float(np.max([r["A2"] for r in rows])),
            "A2_meshmedian_over_A2_p50": float(np.median(
                [r["A2_meshmedian"] / r["A2"] for r in rows])),
            "block_p95_over_p05_max": float(np.max([r["blk_block_sigma_p95_over_p05"] for r in rows])),
            "n_verdict_ok_or_diag": int(counts.get("ok", 0) + counts.get("diagnostic_only", 0)),
            "note": "真实臂无真值 ⇒ 不参与'不恒红'判据；只报分布与量级。"}


def fault_injections(rng: np.random.Generator) -> Dict[str, Any]:
    """三类故障注入必须判红（可执行，不靠叙述）。"""
    shape = (512, 512)
    sig = 20.0
    res: Dict[str, Any] = {}
    for tag, corr, ratio in (("large_scale_corr128_r10", 128.0, 10.0),
                             ("ramp_r100", None, 100.0)):
        s = (C.struct_ramp(shape, ratio * sig * np.sqrt(12.0)) if corr is None
             else C.struct_smooth_field(shape, ratio * sig, corr, rng))
        img = C.add_noise_adu(s, sig, rng)
        p = C.structure_proxies(img, box=32, filter_size=3, n_iter=2)
        a2_id = p["prod_sigma"] / p["sigma_diff_over_sqrt2"]      # 恒等修法
        # 整帧 mesh（box = 帧宽 ⇒ 1×1 mesh ⇒ 背景退化为常数 ⇒ 等价于生产全局做法）
        m_full = C.mesh_sigma(img, box=shape[0], filter_size=1, n_iter=1)["sigma_resid"]
        a2_full = m_full / p["sigma_diff_over_sqrt2"]
        v_ok = C.classify(p["R_struct"], p["C"], p["kf"], delta_budget=DELTA_BUDGET,
                          A1=p["A1"], A2=p["A2"], D=p["D"])
        v_id = C.classify(p["R_struct"], p["C"], p["kf"], delta_budget=DELTA_BUDGET,
                          A1=p["A1"], A2=a2_id, D=p["D"])
        v_full = C.classify(p["R_struct"], p["C"], p["kf"], delta_budget=DELTA_BUDGET,
                            A1=p["A1"], A2=a2_full, D=p["D"])
        v_disabled = "ok"       # 故障：门被短路成恒 ok
        true_excess_id = abs(C.rel_err(p["prod_sigma"], sig) + 0.013)
        res[tag] = {
            "verdict_normal_fix": v_ok,
            "verdict_identity_fix": v_id,
            "verdict_global_mesh_fix": v_full,
            "verdict_gate_disabled": v_disabled,
            "true_excess_identity_fix": true_excess_id,
            "G_identity_fix_red": bool(v_id == "fail_closed"),
            "G_global_mesh_red": bool(v_full == "fail_closed"),
            "G_gate_disabled_would_certify_bad_frame": bool(
                v_disabled == "ok" and true_excess_id > DELTA_BUDGET),
        }
    return res


def candidate_table(d: Dict[str, Any]) -> List[Dict[str, Any]]:
    """候选修法在解析臂上的定量对照（box=32）。

      F0  = 生产全局 2 轮裁剪 RMS（现状，被审计对象）
      F1a = mesh σ 图跨 mesh **中值**（SExtractor back.c:846 backsig 口径）
      F1b = mesh 背景扣除后**残差的全帧裁剪 RMS**（本单元推荐口径）
      F2  = provenance 三档门（与 F1a/F1b 正交，可叠加）
    误差一律相对**真值** σ_true，并给出相对"无结构基线"的**结构增量 excess**。
    """
    rows = d["analytic"]["rows"]
    base = {m: {"f1a": None, "f1b": None} for m in {r["model"] for r in rows}}
    for r in rows:
        if r["struct_over_noise"] == 0.0:
            b = r["boxes"]["32"]
            base[r["model"]] = {"f1a": b["fix_rel_err_meshmedian"],
                                "f1b": b["fix_rel_err"]}
    out = []
    for r in rows:
        b = r["boxes"]["32"]
        bl = base[r["model"]]
        out.append({
            "model": r["model"], "r": r["struct_over_noise"],
            "F0_prod_rel": r["prod_rel_err"],
            "F1a_rel": b["fix_rel_err_meshmedian"],
            "F1b_rel": b["fix_rel_err"],
            "F1a_excess": (b["fix_rel_err_meshmedian"] - bl["f1a"]
                           if bl["f1a"] is not None else None),
            "F1b_excess": (b["fix_rel_err"] - bl["f1b"]
                           if bl["f1b"] is not None else None),
            "verdict_F2": b["verdict"],
        })
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="results")
    ap.add_argument("--out", default="results/exp02_e4_gates.json")
    args = ap.parse_args()
    R = Path(args.results)
    out: Dict[str, Any] = {"delta_budget": DELTA_BUDGET,
                           "certified_verdicts": list(CERTIFIED)}
    d1 = _load(R / "exp02_e1_analytic.json")
    d2 = _load(R / "exp02_e2_hst.json")
    d3 = _load(R / "exp02_e3_real.json")
    if d1:
        out["analytic"] = audit_analytic(d1)
        out["negatives_analytic"] = d1["negatives"]
        out["red_injections_analytic"] = d1["red_injections"]
        out["crosscheck_mirror"] = d1["crosscheck_mirror"]
        out["candidates"] = candidate_table(d1)
    if d2:
        out["hst"] = audit_hst(d2)
    if d3:
        out["real"] = audit_real(d3)
    out["fault_injections"] = fault_injections(np.random.default_rng(20260925))
    # 全局判定
    gates = []
    for k in ("analytic", "hst"):
        if k in out and "G_no_false_certification_pass" in out[k]:
            gates.append((k + ".no_false_certification", out[k]["G_no_false_certification_pass"]))
    if d1:
        gates.append(("analytic.null_zero", d1["negatives"]["G_null_zero_pass"]))
        gates.append(("analytic.gate_not_always_red",
                      d1["negatives"]["G_gate_not_always_red_pass"]))
        gates.append(("analytic.gate_not_always_green",
                      out["analytic"]["verdict_counts"].get("fail_closed", 0) > 0))
    if d2:
        gates.append(("hst.null_zero", d2["negatives_no_base"]["G_null_zero_pass"]))
        vc = out["hst"]["verdict_counts"]
        gates.append(("hst.gate_not_always_green", vc.get("fail_closed", 0) > 0))
        gates.append(("hst.gate_not_always_red", vc.get("ok", 0) > 0))
    for tag, v in out["fault_injections"].items():
        gates.append((tag + ".identity_fix_red", v["G_identity_fix_red"]))
        gates.append((tag + ".global_mesh_red", v["G_global_mesh_red"]))
        gates.append((tag + ".gate_disabled_certifies_bad",
                      v["G_gate_disabled_would_certify_bad_frame"]))
    out["GATES"] = {k: bool(v) for k, v in gates}
    out["ALL_GATES_PASS"] = bool(all(v for _, v in gates))
    p = Path(args.out)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(out, ensure_ascii=False, indent=1, default=float),
                 encoding="utf-8")
    print(json.dumps({"GATES": out["GATES"], "ALL_GATES_PASS": out["ALL_GATES_PASS"],
                      "analytic": {k: v for k, v in out.get("analytic", {}).items()
                                   if k != "violations"},
                      "hst": {k: v for k, v in out.get("hst", {}).items()
                              if k != "violations"},
                      "real": out.get("real")}, ensure_ascii=False, indent=1,
                     default=float))
    print("wrote", p)
    return 0 if out["ALL_GATES_PASS"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
