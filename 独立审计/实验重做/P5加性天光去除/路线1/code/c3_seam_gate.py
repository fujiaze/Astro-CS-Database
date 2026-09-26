#!/usr/bin/env python3
"""C3 -- 接缝判据门槛 max_e |rel_step(e)| <= 1e-2 三腿之实验腿.

验证对象: docs/science/PHASE2_UPM.md §9a/§17, docs/plugins/algorithms_phase2/11_upm.md §4.1
  rel_step(e) = median_i [I(p+d·n) - I(p-d·n)] / bg(e),  d = 2 px, N_e = 256.
  声称: (a) 无噪声确定性下限 Δ/L > gate/(1-gate/2) = 1.0050% 必判红;
        (b) H0 下的 σ_rel = sqrt(pi/2)·MAD(seam)/(sqrt(N)·bg) 描述判据量散布;
        (c) 检出概率带多重性 (n_s=1..4), 单边预测系统性偏低;
        (d) 平滑过渡宽度 w>2d 把等效门限抬高 ~w/2d;
        (e) 反号梯度与台阶代数相消 => 1.005% 台阶判绿 (已知漏检面, 01 D-57);
        (f) 负例: 无台阶无梯度 => 判据量回落到噪声基线, 判绿.

方法: 纯 numpy MC, 固定 seed. 噪声取 M42 同量级 sigma_pix/bg = 1.0703e-2.
"""
import json
import math
import os
import numpy as np

SEED = 20260319
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c3_seam_gate.json")
GATE = 1e-2
D = 2.0            # 采样半距 (px)
N_E = 256          # 每边采样点
BG = 1.0           # 归一化局部背景电平
SIGMA_PIX = 1.0703e-2   # M42 实测量级: sigma_pix/bg
N_MC = 4000


def rel_step_median(seam, bg):
    return float(np.median(seam)) / bg


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "gate": GATE, "d": D, "n_e": N_E, "bg": BG,
           "sigma_pix_over_bg": SIGMA_PIX, "n_mc": N_MC}

    # ---- (a) 确定性下限: 无噪声, 台阶 Δ, bg = L + Δ/2 ----
    floor = {}
    for dL in (0.010049, 0.0100503, 0.010051, 0.011):
        L = 1.0
        Dlt = dL * L
        bg = L + Dlt / 2.0
        rs = Dlt / bg
        floor[f"{dL:.6f}"] = {"rel_step": rs, "red": bool(abs(rs) > GATE)}
    pred = GATE / (1.0 - GATE / 2.0)
    floor["closed_form_gate_over_1_minus_gate_over_2"] = pred
    res["deterministic_floor"] = floor

    # ---- (b) H0 散布: 无台阶, 无梯度; seam = eps+ - eps- ----
    seams = rng.standard_normal((N_MC, N_E)) * SIGMA_PIX * BG * math.sqrt(2.0)
    rels = np.median(seams, axis=1) / BG
    mad = float(np.median(np.abs(seams - np.median(seams)))) * 1.4826
    sigma_rel_pred = math.sqrt(math.pi / 2.0) * mad / (math.sqrt(N_E) * BG)
    res["h0_null"] = {
        "sigma_rel_measured": float(np.std(rels)),
        "sigma_rel_formula": sigma_rel_pred,
        "ratio_measured_over_formula": float(np.std(rels)) / sigma_rel_pred,
        "max_abs_rel_step_p999": float(np.quantile(np.abs(rels), 0.999)),
        "false_alarm_rate_at_gate": float(np.mean(np.abs(rels) > GATE)),
    }

    # ---- (c) 检出概率与多重性 ----
    # sigma_per_edge (判据量逐边散布) 取 (b) 的公式值; 解析 sigma_rel = 1.187e-3 对应
    # sigma_pix/bg 更小; 这里用本 fixture 自洽数值.
    sig_e = sigma_rel_pred
    det = {}
    for n_s in (1, 2, 4):
        row = {}
        for dL in (0.0090, 0.0095, 0.0100, 0.0105, 0.0110):
            L, Dlt = 1.0, dL
            bg = L + Dlt / 2.0
            mu = Dlt / bg
            # 解析(含多重性): P_det = 1 - prod_e Phi((gate - mu)/sigma_e)
            from math import erf
            p1 = 0.5 * (1.0 + erf((GATE - mu) / (sig_e * math.sqrt(2.0))))
            p_analytic = 1.0 - p1 ** n_s
            # MC: n_s 条边各自独立采样, 判决取 max |rel|
            hit = 0
            for _ in range(N_MC):
                e = rng.standard_normal(n_s) * sig_e + mu
                if np.max(np.abs(e)) > GATE:
                    hit += 1
            row[f"dL={dL:.4f}"] = {"analytic_multiplicity": p_analytic,
                                   "mc": hit / N_MC,
                                   "single_edge_analytic": 1.0 - p1}
        det[f"n_s={n_s}"] = row
    res["detection"] = det

    # ---- (d) 平滑过渡: 台阶在宽度 w 上线性过渡 ----
    smooth = {}
    for w in (4.0, 8.0, 16.0, 32.0):
        # seam(+d) - seam(-d) 看到的等效台阶 = Δ * (2d/w) (w>2d 时)
        Dlt = 0.02  # 2% 名义台阶
        L = 1.0
        prof = np.linspace(0.0, Dlt, int(w) + 1)  # 过渡剖面
        # ±d 采样: 边界在剖面中央
        i = int(w) // 2
        off_lo = prof[max(0, i - int(D))]
        off_hi = prof[min(int(w), i + int(D))]
        seen = off_hi - off_lo
        bg = L + Dlt / 2.0
        eff = (L + seen) - L  # 观察到的台阶
        rs = eff / (bg)
        smooth[f"w={int(w)}px"] = {"seen_step_frac_of_nominal": seen / Dlt,
                                   "rel_step": rs, "red": bool(abs(rs) > GATE),
                                   "threshold_inflation_w_over_2d": w / (2 * D)}
    res["smooth_transition"] = smooth

    # ---- (e) 反号梯度相消 (D-57 漏检面) ----
    # 背景 L - g·x (向边界递减), seam = I(+d) - I(-d) = Δ - 2d·g·(bg 口径)
    # 梯度贡献 rel = -2d·g/bg; 取 |2d·g/bg| = Δ/L·bg/(L) 使相消.
    grad = {}
    dL = pred  # 1.0050% 台阶
    Dlt = dL
    g2d_over_bg = dL  # 梯度项 2d·g = dL·bg => 相消
    seam_vals = np.full(N_E, Dlt - g2d_over_bg) + rng.standard_normal(N_E) * SIGMA_PIX * BG * math.sqrt(2.0)
    bg = 1.0 + Dlt / 2.0
    rs = rel_step_median(seam_vals, bg)
    grad["cancelled_injection"] = {"rel_step": rs, "red": bool(abs(rs) > GATE),
                                   "nominal_dL": dL,
                                   "verdict": "判绿 => 已知漏检面复现" if abs(rs) <= GATE else "判红"}
    # 无相消对照
    seam_vals2 = np.full(N_E, Dlt) + rng.standard_normal(N_E) * SIGMA_PIX * BG * math.sqrt(2.0)
    rs2 = rel_step_median(seam_vals2, bg)
    grad["no_gradient_control"] = {"rel_step": rs2, "red": bool(abs(rs2) > GATE)}
    res["gradient_cancellation"] = grad

    # ---- (f) 负例: 真值无台阶 => 度量归零 (噪声基线) ----
    seams0 = rng.standard_normal((N_MC, N_E)) * SIGMA_PIX * BG * math.sqrt(2.0)
    maxs = np.max(np.abs(np.median(seams0, axis=1) / BG))
    res["negative_null_case"] = {
        "median_rel_step": 0.0,
        "max_over_mc_of_max_e": float(maxs),
        "all_green": bool(np.all(np.abs(np.median(seams0, axis=1) / BG) <= GATE)),
        "note": "真值无效应 => rel_step 中位数归零, 判决量只含噪声, 门在 1e-2 处应全绿",
    }

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
