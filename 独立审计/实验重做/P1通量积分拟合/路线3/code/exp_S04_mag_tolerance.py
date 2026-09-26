#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S04 星等预过滤窗宽 mag_tolerance=3.0 mag（PHOTOMETRY.md:219-222, 式-2）。

假说 H4a(零点平移不变量/尺度不变性, V-5): 全体 F_instr 同乘 k ⇒
        location += log10 k、k_photo /= k、sigma_residual 与内点集逐位不变。
假说 H4b: 清洁场(无错配)下窗宽对 k_photo 的影响恒为 0（真值无效应⇒度量归零）；
        污染场下窗宽 ≥ 与 IRLS 联合给出无偏 k_photo, 窗宽的作用只是放走数量级级错配。
seed 固定 = 20260926。复现: python3 exp_S04_mag_tolerance.py
"""
import json, os
import numpy as np

SEED = 20260926
C, MADS = 4.685, 0.6744897501960817


def pipeline(F_instr, F_syn, G, mag_tol):
    """式-2 + 式-3 的最小可执行复刻（不含质量位, 与本实验无关）."""
    r = np.log10(F_instr / F_syn)
    delta = -2.5 * np.log10(F_instr) - G
    med = np.median(delta)
    keep = np.abs(delta - med) <= mag_tol
    rc = r[keep]
    if rc.size < 3:
        return {"scale": 1.0, "sigma": 0.0, "n": int(rc.size), "keep": keep}
    loc = float(np.median(rc))
    mad = float(np.median(np.abs(rc - loc)))
    s = mad / MADS if mad > 0 else 0.0
    if s > 0:
        for _ in range(50):
            u = (rc - loc) / (C * s)
            w = (1 - u * u) ** 2 * (np.abs(u) < 1)
            sw = w.sum()
            if sw <= 0:
                break
            new = float(np.dot(w, rc) / sw)
            if abs(new - loc) < 1e-6:
                loc = new
                break
            loc = new
    scale = 10.0 ** (-loc)
    rin = rc  # 内点集按 |r-loc|<cS 的近似由 w>0 给出; 本实验用 w>0
    u = (rc - loc) / (C * s) if s > 0 else np.zeros_like(rc)
    rin = rc[np.abs(u) < 1] if s > 0 else rc
    sigma = float(np.median(np.abs(rin - np.median(rin))) / MADS) if rin.size >= 2 else 0.0
    return {"scale": scale, "sigma": sigma, "n": int(rc.size), "keep": keep}


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED}
    n = 400
    loc_true = -0.35                      # 真实 dex 偏移
    G = rng.uniform(8.5, 14.5, n)
    # 真实性修正: mag_syn ≈ G + 小散度(F_syn 与 G 强相关), 否则 delta 散布被 G 主导、
    # 预过滤窗变成对 r 的选择性截断 —— 生产中 delta = mag_syn − ZP − G + 2.5·noise,
    # 真实散布 ~0.05-0.1 mag(PHOTOMETRY.md §2a.6), 3.0 mag 窗对清洁场恒不截
    mag_syn = G + rng.normal(0, 0.03, n)
    ZP_true = -15.13
    F_syn = 10.0 ** (-0.4 * (mag_syn - ZP_true))
    F_instr = F_syn * 10.0 ** loc_true * 10.0 ** rng.normal(0, 0.015, n)
    out["H4a"] = {}
    for k in (1.0, 10 ** 0.37, 10 ** -1.11):
        res_k = pipeline(F_instr * k, F_syn, G, 3.0)
        res_1 = pipeline(F_instr, F_syn, G, 3.0)
        out["H4a"][f"k={k:.4f}"] = {
            "location_shift": float(np.log10(k) + np.log10(res_1["scale"] * 10 ** 0 / res_k["scale"]) * 0)
        }
    # 精确断言: scale(k)/scale(1) = 1/k; sigma 不变
    sc = []
    sg = []
    keep_ids = []
    for k in (1.0, 10 ** 0.37):
        r_k = pipeline(F_instr * k, F_syn, G, 3.0)
        sc.append(r_k["scale"]); sg.append(r_k["sigma"]); keep_ids.append(r_k["keep"])
    out["H4a"] = {
        "scale_ratio_actual": sc[1] / sc[0],
        "scale_ratio_expected_1_over_k": 1.0 / (10 ** 0.37),
        "sigma_rel_diff": abs(sg[1] - sg[0]) / sg[0],
        "keep_mask_identical": bool(np.array_equal(keep_ids[0], keep_ids[1])),
        "pass": bool(abs(sc[1] / sc[0] - 1.0 / (10 ** 0.37)) < 1e-12
                     and abs(sg[1] - sg[0]) / sg[0] < 1e-9),
    }

    # H4b: 清洁场 ⇒ 窗宽无效应(归零); 污染场 ⇒ 窗宽+IRLS 联合无偏
    widths = [0.5, 1.0, 2.0, 3.0, 6.0, 100.0]
    clean = {w: pipeline(F_instr, F_syn, G, w)["scale"] for w in widths}
    base = clean[3.0]
    out["H4b_clean"] = {
        "k_photo_by_width": {str(w): v / base for w, v in clean.items()},
        "max_dev_from_w3": float(max(abs(v / base - 1) for v in clean.values())),
        "criterion": "真值无效应 ⇒ 全部窗宽给出同一 k_photo(偏差=0)",
        "pass": bool(max(abs(v / base - 1) for v in clean.values()) < 1e-12),
    }
    # 污染: 15% 错配星, 错配幅度 1.5-5 mag (即 0.6-2 dex)
    n_bad = int(0.15 * n)
    bad = rng.choice(n, n_bad, replace=False)
    F_con = F_instr.copy()
    F_con[bad] *= 10.0 ** rng.uniform(-2.0, 2.0, n_bad)   # 错配倍率
    cont = {}
    for w in widths:
        r = pipeline(F_con, F_syn, G, w)
        cont[w] = {"k_bias_dex": float(abs(np.log10(r["scale"] / base))),
                   "sigma": r["sigma"], "n_kept": r["n"]}
    out["H4b_contaminated"] = {
        "table": {str(w): v for w, v in cont.items()},
        "note": "错配幅度 >1.2 dex 者 >6 mag ⇒ 3.0 窗先剔; ≤1.2 dex 者由 IRLS 截断",
        "pass": bool(cont[3.0]["k_bias_dex"] < 0.01),
    }

    out["verdict"] = {"H4a": out["H4a"]["pass"], "H4b_clean": out["H4b_clean"]["pass"],
                      "H4b_contaminated": out["H4b_contaminated"]["pass"]}
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S04_mag_tolerance.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({"H4a": out["H4a"], "H4b_clean": out["H4b_clean"],
                      "H4b_contaminated": out["H4b_contaminated"], "verdict": out["verdict"]},
                     ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
