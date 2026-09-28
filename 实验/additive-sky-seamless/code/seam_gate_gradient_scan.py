# -*- coding: utf-8 -*-
"""P5 接缝门的第三失效模式（光滑法向斜坡）独立探针 + 斜坡不敏感化备选判据。

纪律
====
- **只读**调用生产门本体 `eng/tools/e2e/seam_footprint.py` 的 `edge_metric` /
  `gate_decision`（判据不重写、不复制；正负例与生产同一条代码路径）。
- 本脚本不改任何门本体、不改判据面；"斜坡不敏感化"量只作为**备选判据的证据**落盘，
  是否入门由判据面变更流程裁决（见 docs/science/PHASE2_UPM.md §17.5）。
- 固定 seed；无网络；无时间/环境随机源。

判据的量纲（本探针要量化的对象）
================================
门判据量 rel_step = median(seam)/bg，seam = I(p+d·n) − I(p−d·n)，d = 2 px。
把边界处剖面按法向展开 I(n) = L + n·∂L/∂n（一阶）：
    seam = 2d·∂L/∂n + Δ（Δ = 真实电平台阶）
⇒ rel_step = 2d·ρ + Δ/bg，  ρ ≡ (∂L/∂n)/bg = 边界处**相对**法向斜率 [1/px]
⇒ **真值无台阶（Δ=0）时判据量仍非零**：ρ = 0.25%/px 恰好把 rel_step 推到门 1e-2。
d 扫描诊断量 rel_step_d4x 用 4d = 8 px：台阶项不变、斜坡项 ×4（比值 4 = 纯斜坡特征）。

备选判据（线性外推消梯度项）
============================
seam(d) = Δ + 2d·g（一阶展开，g = ∂L/∂n）⇒ 取 d 与 4d 两次读数：
    Δ_hat = (4·seam(d) − seam(4d)) / 3        （与 d 无关）
    rel_step_nograd = Δ_hat / bg              （**同一个** bg，与门同口径）
纯斜坡 ⇒ Δ_hat ≡ 0（与 g 无关）；真实台阶 ⇒ Δ_hat = Δ。

复现：python3 实验/additive-sky-seamless/code/seam_gate_gradient_scan.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

UNIT = Path(__file__).resolve().parents[1]           # 实验/additive-sky-seamless/
ROOT = UNIT.parents[1]                               # 仓库根
sys.path.insert(0, str(ROOT / "eng" / "tools" / "e2e"))
from seam_footprint import edge_metric, gate_decision  # noqa: E402  （生产门本体，只读）

SEED = 20260928          # 本探针专属 seed
GATE = 1e-2              # SCI-C R5 相对接缝门（判据面唯一阈值，本脚本不改）
D = 2.0
CTRL_SHIFT = 200.0
MIN_SAMPLES = 20

H = W = 1200
X0, X1 = 250, 1150
Y0, Y1 = 250, 1150
XB = 700                 # 被检竖边（法向 = −x̂）
BG = 1.0


def boundary():
    y = np.arange(300.0, 1101.0, 4.0)
    return np.stack([np.full(y.size, float(XB)), y], axis=1)


def image(delta_frac: float = 0.0, slope_frac_per_px: float = 0.0) -> np.ndarray:
    """只含 x 方向剖面：边界处电平 = BG；法向**相对**斜率 = slope_frac_per_px/px。

    剖面 = BG·[1 + slope·(x − XB)] + Δ·1{x ≥ XB}（Δ = delta_frac·BG）。
    """
    img = np.full((H, W), np.nan)
    xx = np.arange(X0, X1, dtype=float)
    prof = BG * np.ones(xx.size)
    if slope_frac_per_px:
        prof = prof * (1.0 + slope_frac_per_px * (xx - XB))
    if delta_frac:
        prof = prof + np.where(xx < XB, 0.0, delta_frac * BG)
    img[Y0:Y1, X0:X1] = prof[None, :]
    return img


def measure(img: np.ndarray) -> dict:
    m = edge_metric(img, boundary(), d=D, ctrl_shift=CTRL_SHIFT, min_samples=MIN_SAMPLES)
    m["frame"], m["edge"] = "SYNTH", "v"
    s2, s8, bg = m.get("step"), m.get("step_d4x"), m.get("bg")
    if s2 is not None and s8 is not None and bg:
        m["step_nograd"] = (4.0 * s2 - s8) / 3.0
        m["rel_step_nograd"] = m["step_nograd"] / bg
        m["d4x_over_d_ratio"] = (s8 / s2) if s2 else None
    else:
        m["step_nograd"] = m["rel_step_nograd"] = m["d4x_over_d_ratio"] = None
    return m


def case(name: str, m: dict, asserts) -> dict:
    g = gate_decision([m], GATE, MIN_SAMPLES)
    checks = [dict(assertion=a, ok=bool(ok), detail=d) for a, ok, d in asserts(m, g)]
    return dict(case=name, verdict=g["verdict"], rel_step=m["rel_step"], step=m["step"], bg=m["bg"],
                n=m["n"], interior=m["interior"], rel_step_d4x=m["rel_step_d4x"],
                d4x_over_d_ratio=m["d4x_over_d_ratio"], step_nograd=m["step_nograd"],
                rel_step_nograd=m["rel_step_nograd"], checks=checks)


def main() -> int:
    res = {"schema": "SCI-C/SEAM-GRADIENT-SCAN/1", "seed": SEED, "gate": GATE, "d": D,
           "ctrl_shift": CTRL_SHIFT, "min_samples": MIN_SAMPLES,
           # 门被纯梯度推动的解析阈值：rel_step = 2d·ρ = gate ⇒ ρ = gate/(2d)
           "rho_gate_frac_per_px": GATE / (2.0 * D),
           "rho_gate_pct_per_px": 100.0 * GATE / (2.0 * D),
           "note": ("门的第三失效模式（光滑法向斜坡）量化 + 斜坡不敏感化备选判据的正负例；"
                    "备选判据是否入门由判据面变更流程裁决，本脚本不改门本体。"),
           "cases": {}}

    # ── 负例：真值无效应 ⇒ 必须归零（判据非恒真）────────────────────────
    m = measure(image())
    res["cases"]["N1_flat"] = case("N1 平坦（无台阶无斜坡）", m, lambda m, g: [
        ("rel_step 精确归零", m["rel_step"] == 0.0, m["rel_step"]),
        ("rel_step_nograd 精确归零", m["rel_step_nograd"] == 0.0, m["rel_step_nograd"]),
        ("判绿", g["verdict"] == "PASS", g["verdict"])])

    # ── 正例：真实台阶，门与备选判据都必须看见 ──────────────────────────
    m = measure(image(delta_frac=1.5e-2))
    res["cases"]["P1_step_1.5pct"] = case("P1 真实台阶 1.5%", m, lambda m, g: [
        ("门判红", g["verdict"] == "FAIL", g["verdict"]),
        ("备选判据同样红（不误伤真台阶）", abs(m["rel_step_nograd"]) > GATE, m["rel_step_nograd"]),
        ("d 扫描比值 = 1（台阶在 d 扫描下守恒）",
         abs(m["d4x_over_d_ratio"] - 1.0) < 1e-9, m["d4x_over_d_ratio"])])

    m = measure(image(delta_frac=0.8e-2))
    res["cases"]["P2_step_0.8pct"] = case("P2 真实台阶 0.8%（< 确定性地板 1.0050251%）", m,
                                          lambda m, g: [
        ("门判绿（地板行为正确，不是漏检面）", g["verdict"] == "PASS", g["verdict"]),
        ("备选判据同样绿", abs(m["rel_step_nograd"]) <= GATE, m["rel_step_nograd"])])

    # ── 第三失效模式：光滑法向斜坡，真值无台阶 ──────────────────────────
    m = measure(image(slope_frac_per_px=1.0e-3))
    res["cases"]["F1_ramp_rho_0.10pct_per_px"] = case("F1 光滑斜坡 ρ=0.10%/px（无台阶）", m,
                                                      lambda m, g: [
        ("门判据量被梯度污染（非零、仍 < 门）", 0.0 < abs(m["rel_step"]) < GATE, m["rel_step"]),
        ("d4x 诊断量**超门**（无接缝却超门）", abs(m["rel_step_d4x"]) > GATE, m["rel_step_d4x"]),
        ("d4x/d = 4（纯斜坡特征，对比正例的 1）",
         abs(m["d4x_over_d_ratio"] - 4.0) < 1e-9, m["d4x_over_d_ratio"]),
        ("备选判据归零（斜坡不敏感）", abs(m["rel_step_nograd"]) < 1e-12, m["rel_step_nograd"])])

    m = measure(image(slope_frac_per_px=2.6e-3))
    res["cases"]["F2_ramp_rho_0.26pct_per_px"] = case(
        "F2 光滑斜坡 ρ=0.26%/px（无台阶）⇒ 门自身伪阳", m, lambda m, g: [
            ("门判红（真值无接缝 ⇒ 伪阳/误伤）", g["verdict"] == "FAIL", g["verdict"]),
            ("备选判据仍绿（不误伤）", abs(m["rel_step_nograd"]) <= GATE, m["rel_step_nograd"])])

    # ── 已登记漏检面 (ii)：反号梯度相消 ⇒ 备选判据能救回 ─────────────────
    delta = 1.05e-2                       # 名义 1.05% > 确定性地板 1.0050251%
    slope_cancel = -delta / (2.0 * D)     # 2d·g = −Δ ⇒ seam(d) ≡ 0
    m = measure(image(delta_frac=delta, slope_frac_per_px=slope_cancel))
    res["cases"]["F3_antiphase_cancel_1.05pct"] = case(
        "F3 1.05% 台阶 + 反号梯度相消（漏检面 ii）", m, lambda m, g: [
            ("门读 0 判绿（漏检面 ii 复现）", abs(m["rel_step"]) < 1e-12, m["rel_step"]),
            ("备选判据恢复台阶幅度 Δ/bg（该漏检面被消除）",
             abs(abs(m["rel_step_nograd"]) - delta / m["bg"]) < 1e-12,
             (m["rel_step_nograd"], delta / m["bg"])),
            ("d4x 诊断量也超门（另一路旁证）", abs(m["rel_step_d4x"]) > GATE, m["rel_step_d4x"])])

    # ── 台阶 + 斜坡叠加：备选判据只扣斜坡项、不扣台阶 ────────────────────
    m = measure(image(delta_frac=1.5e-2, slope_frac_per_px=1.0e-3))
    res["cases"]["P3_step_1.5pct_plus_ramp_rho_0.10pct"] = case(
        "P3 1.5% 台阶 + 0.10%/px 斜坡", m, lambda m, g: [
            ("门判红", g["verdict"] == "FAIL", g["verdict"]),
            ("备选判据恢复台阶幅度（仍 > 门）", abs(m["rel_step_nograd"]) > GATE,
             m["rel_step_nograd"]),
            ("备选判据 ≈ 台阶真值/bg",
             abs(abs(m["rel_step_nograd"]) - 1.5e-2 / m["bg"]) < 1e-12,
             (m["rel_step_nograd"], 1.5e-2 / m["bg"]))])

    # ── 噪声面：两判据的散布与超门率（250 采样/边，400 次 MC，固定 seed）──
    rng = np.random.default_rng(SEED)
    sigma = 1.0e-3 * BG
    n_mc = 400
    acc = {"flat": [], "ramp": [], "step": []}
    for _ in range(n_mc):
        for tag, d_frac, slope in (("flat", 0.0, 0.0),
                                   ("ramp", 0.0, 2.6e-3),
                                   ("step", 1.5e-2, 0.0)):
            img = image(delta_frac=d_frac, slope_frac_per_px=slope)
            img[Y0:Y1, X0:X1] += rng.normal(0.0, sigma, size=(Y1 - Y0, X1 - X0))
            mm = measure(img)
            acc[tag].append((mm["rel_step"], mm["rel_step_nograd"], mm["bg"]))
    noise = {}
    for tag, vals in acc.items():
        a = np.array([v[0] for v in vals]); b = np.array([v[1] for v in vals])
        truth = 0.0 if tag != "step" else float(np.mean([1.5e-2 / v[2] for v in vals]))
        noise[tag] = {
            "rel_step_mean": float(a.mean()), "rel_step_std": float(a.std(ddof=1)),
            "rel_step_nograd_mean": float(b.mean()), "rel_step_nograd_std": float(b.std(ddof=1)),
            "rel_step_exceed_rate": float((np.abs(a) > GATE).mean()),
            "rel_step_nograd_exceed_rate": float((np.abs(b) > GATE).mean()),
            # 两量化都是**有符号**量且法向为 −x̂ ⇒ 用 |中位| 与真值幅值比（避免符号约定混淆）
            "abs_bias_delivered": float(abs(a.mean()) - truth),
            "abs_bias_nograd": float(abs(b.mean()) - truth),
            "noise_std_ratio_nograd_over_delivered": float(b.std(ddof=1) / a.std(ddof=1)),
        }
    ok_noise = (noise["flat"]["rel_step_nograd_exceed_rate"] == 0.0
                and noise["ramp"]["rel_step_nograd_exceed_rate"] == 0.0
                and noise["step"]["rel_step_nograd_exceed_rate"] >= 0.5)
    res["noise_mc"] = dict(n_mc=n_mc, sigma_over_bg=sigma, per_case=noise,
                           assertion=dict(ok=bool(ok_noise),
                                          detail=("负例（平坦/斜坡）备选判据超门率必须为 0；"
                                                  "正例（真台阶）超门率 ≥ 0.5")))

    all_checks = [c for cs in res["cases"].values() for c in cs["checks"]]
    res["summary"] = {"n_assertions": len(all_checks),
                      "n_failed": int(sum(1 for c in all_checks if not c["ok"])),
                      "noise_assertion_ok": bool(ok_noise),
                      "verdict": "PASS" if (all(c["ok"] for c in all_checks) and ok_noise) else "FAIL"}
    out = UNIT / "results" / "seam_gate_gradient_scan.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(res, indent=1, ensure_ascii=False, sort_keys=True), encoding="utf-8")
    print(json.dumps(res["summary"], ensure_ascii=False))
    for name, cs in res["cases"].items():
        print("%-40s gate=%-4s rel_step=%+.6g rel_d4x=%+.6g rel_nograd=%+.6g ratio=%s"
              % (name, cs["verdict"], cs["rel_step"] or 0.0, cs["rel_step_d4x"] or 0.0,
                 cs["rel_step_nograd"] or 0.0, cs["d4x_over_d_ratio"]))
        for c in cs["checks"]:
            if not c["ok"]:
                print("    !! FAILED:", c["assertion"], c["detail"])
    print("noise:", json.dumps({k: {kk: vv for kk, vv in v.items()} for k, v in noise.items()},
                               ensure_ascii=False))
    print("wrote", out)
    return 0 if res["summary"]["verdict"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
