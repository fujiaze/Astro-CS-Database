#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q1 接缝判据门槛 max|rel_step| <= 1e-2 的独立三腿复算（路线3, P5 加性天光去除）。

正本: docs/science/PHASE2_UPM.md §9a + §17; docs/plugins/algorithms_phase2/11_upm.md §4.1
观测量（§17.1 逐字同源）:
    rel_step(e) = median_i seam_i(e) / bg(e)
    seam_i      = I(p_i + d*n) - I(p_i - d*n)
    bg(e)       = median(|I| over the 2N ±d 采样点)
固定 seed: SEED = 20260926。真值无效应负例: amp=0 且 grad=0 => rel_step 精确为 0。
纯 Python + numpy；不 import 仓库任何 Python。
运行: python3 exp01_seam_gate.py   (工作目录任意; 结果写 ../results/q1_seam_gate.json)
"""
import json
import os
import numpy as np

SEED = 20260926
GATE = 1e-2
D = 2                 # 采样半距 d (px)
N_EDGE = 256          # 每边采样点数 (§17.1 默认 256)
SIGMA_OVER_BG = 1.0703e-2   # §17.3: 与 M42 同量级 sigma_pix/bg
L = 1000.0            # 边界一侧局部电平（相对判据, 标度任意）
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q1_seam_gate.json")


def samples(L, amp, grad, sigma, rng, n=N_EDGE, d=D, frac=1.0, width=None):
    """边界法向坐标 s: side+ (s>0) 电平 L+amp*chi_i, side- (s<0) 电平 L。
    grad: 背景沿法向线性梯度 (e-/px)。width: 平滑接缝过渡宽度(px), None=硬台阶。
    frac: 边界采样点中承载台阶的比例（部分边界接缝）。
    返回 (seam_i, bg, n_eff)。"""
    x = rng.normal(0.0, sigma, size=(2, n))          # 噪声 (side+, side-)
    chi = (np.arange(n) < frac * n).astype(float)    # 台阶承载掩码
    if width is None:
        step_p, step_m = amp * chi, np.zeros(n)
    else:                                            # 宽 w 线性斜坡: 阶跃差分衰减 2d/w
        att = min(1.0, 2.0 * d / width)
        step_p, step_m = amp * att * chi, np.zeros(n)
    seam = (L + step_p + grad * d + x[0]) - (L + step_m - grad * d + x[1])
    I_all = np.concatenate([L + step_p + grad * d + x[0],
                            L + step_m - grad * d + x[1]])
    bg = np.median(np.abs(I_all))
    return seam, bg, n


def metric(**kw):
    rng = kw.pop("rng")
    seam, bg, _ = samples(rng=rng, **kw)
    return np.median(seam) / bg


def main():
    rng0 = np.random.default_rng(SEED)
    out = {"seed": SEED, "gate": GATE, "d": D, "n_edge": N_EDGE,
           "sigma_over_bg": SIGMA_OVER_BG, "cases": {}}

    # ---- (a) 真值无效应负例（必须归零） ----
    m0 = metric(rng=np.random.default_rng(SEED + 1), L=L, amp=0.0, grad=0.0, sigma=0.0)
    out["cases"]["negative_zero_effect"] = {"rel_step": m0, "ok": abs(m0) == 0.0}

    # ---- (b) 确定性下限: 无噪声硬台阶 rel_step = Δ/(L+Δ/2) ----
    # 解析: Δ/L > gate/(1-gate/2); 数值求根核对
    xs = np.linspace(0.0, 0.05, 200001)
    vals = xs / (1.0 + xs / 2.0)                    # rel_step(Δ/L), g=0
    i = np.argmax(vals > GATE)
    floor_num = xs[i]
    floor_ana = GATE / (1.0 - GATE / 2.0)
    out["cases"]["deterministic_floor"] = {
        "analytic_delta_over_L": floor_ana, "numeric_delta_over_L": floor_num,
        "match": abs(floor_num - floor_ana) < 1e-6}

    # ---- (c) 零假设虚警（含噪, amp=0, grad=0）----
    reps, cnt1, cnt4 = 20000, 0, 0
    for r in range(reps):
        ms = [abs(metric(rng=np.random.default_rng(SEED + 100 + r * 8 + j),
                         L=L, amp=0.0, grad=0.0, sigma=SIGMA_OVER_BG * L))
              for j in range(4)]
        cnt1 += ms[0] > GATE
        cnt4 += max(ms) > GATE
    sigma_rel_ana = np.sqrt(np.pi / 2) * np.sqrt(2.0) * SIGMA_OVER_BG / np.sqrt(N_EDGE)
    out["cases"]["false_alarm"] = {
        "sigma_rel_analytic": sigma_rel_ana,
        "k_sigma_gate": GATE / sigma_rel_ana,
        "p_single_edge_empirical": cnt1 / reps,
        "p_family_4edges_empirical": cnt4 / reps}

    # ---- (d) 检出概率曲线（n_s=4 边受影响, 30 seed/点, §17.3 同设计）----
    det = []
    for dl in [0.0090, 0.0095, 0.0100, 0.0105, 0.0110]:
        hits = 0
        for s in range(30):
            ms = [abs(metric(rng=np.random.default_rng(SEED + 1000 + s * 8 + j),
                             L=L, amp=dl * L, grad=0.0, sigma=SIGMA_OVER_BG * L))
                  for j in range(4)]
            hits += max(ms) > GATE
        det.append({"delta_over_L": dl, "p_detect": hits / 30.0})
    out["cases"]["detection_curve_ns4_30seeds"] = det

    # ---- (e) 含多重性的解析检出下限（对照 §17.3 表）----
    from math import erf, sqrt
    import numpy as _np
    _erf = _np.vectorize(lambda z: erf(z))
    Phi = lambda z: 0.5 * (1.0 + _erf(_np.asarray(z, dtype=float) / sqrt(2.0)))
    ana = {}
    for sig_name, sig in [("analytic_1.187e-3", 1.187e-3), ("cross_edge_2.814e-3", 2.814e-3)]:
        row = {}
        for ns in [1, 2, 4]:
            # μ_e = Δ/(L+Δ/2), 检出 = max_e |metric| > gate
            # @50%: Φc((gate-μ)/σ)^ns = 0.5; @95%: Φc((gate-μ)/σ)^ns = 0.05
            xs2 = np.linspace(0.0, 0.03, 300001)
            mu = xs2 / (1.0 + xs2 / 2.0)
            p_det = 1.0 - Phi((GATE - mu) / sig) ** ns + Phi((-GATE - mu) / sig) ** ns
            i50 = np.argmax(p_det >= 0.5)
            i95 = np.argmax(p_det >= 0.95)
            row[f"ns{ns}"] = {"at50": float(xs2[i50]), "at95": float(xs2[i95])}
        ana[sig_name] = row
    out["cases"]["analytic_detection_multiplicity"] = ana

    # ---- (f) 梯度反号相消（D-57 漏检面）----
    gf = -0.0072  # 2d*grad/L = -0.72%: 反号梯度项
    xs3 = np.linspace(0.0, 0.05, 500001)
    met = (xs3 * L + gf * L) / (L + xs3 * L / 2.0)
    ok = np.abs(met) > GATE
    miss_floor = xs3[np.argmax(ok)] if ok.any() else None
    # 正号梯度（误红面）: Δ/L=0.5% 是否已判红
    met_pos = (0.005 * L + 0.0072 * L) / (L + 0.0025 * L)
    out["cases"]["gradient_sign"] = {
        "grad_frac": gf,
        "anti_sign_miss_floor_delta_over_L": float(miss_floor) if miss_floor else None,
        "claim_1.73pct": bool(miss_floor and abs(miss_floor - 0.0173) < 0.002),
        "metric_at_1.005pct_anti": float((0.01005 * L + gf * L) / (L + 0.01005 * L / 2.0)),
        "pos_grad_metric_at_0.505pct": float(met_pos),
        "pos_grad_false_red": bool(met_pos > GATE)}

    # ---- (g) 中位数结构性盲区: 只有比例 f 的边界承载台阶 ----
    blind = []
    for f in [0.3, 0.4, 0.5, 0.6]:
        m = metric(rng=np.random.default_rng(SEED + 77), L=L, amp=0.05 * L,
                   grad=0.0, sigma=SIGMA_OVER_BG * L, frac=f)
        m0 = metric(rng=np.random.default_rng(SEED + 78), L=L, amp=0.05 * L,
                    grad=0.0, sigma=0.0, frac=f)   # 无噪对照: 结构性盲区极限
        blind.append({"frac": f, "rel_step_noisy": float(m),
                      "rel_step_noiseless": float(m0),
                      "noiseless_blind": abs(m0) < 1e-12,
                      "trips_gate_noisy": abs(m) > GATE})
    out["cases"]["median_blindness_step5pct"] = blind

    # ---- (h) 平滑接缝（过渡宽度 w > 2d）----
    smooth = []
    for w in [2.0, 8.0, 16.0, 32.0]:
        # 等效门限应抬升 w/2d 倍: 在 gate*w/2d 附近找翻转
        att = min(1.0, 2.0 * D / w)
        m_below = metric(rng=np.random.default_rng(SEED + 9), L=L,
                         amp=0.9 * GATE * L / att, grad=0.0, sigma=SIGMA_OVER_BG * L, width=w)
        m_above = metric(rng=np.random.default_rng(SEED + 10), L=L,
                         amp=1.1 * GATE * L / att, grad=0.0, sigma=SIGMA_OVER_BG * L, width=w)
        smooth.append({"width_px": w, "effective_gate_scale": w / (2.0 * D),
                       "metric_at_0.9x_eff_gate": float(m_below),
                       "metric_at_1.1x_eff_gate": float(m_above)})
    out["cases"]["smooth_seam"] = smooth

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print(json.dumps(out["cases"], ensure_ascii=False, indent=1,
                     default=lambda o: o.item() if hasattr(o, "item") else str(o))[:3000])
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()