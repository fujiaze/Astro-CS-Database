#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""W3 独立复算：hips_profile=0 与 =1 的不等价（uint8 support 格点量化）+ NaN 分支。

我自己重推误差来源（不抄成稿表）：

  profile=0 (write_hips_direct)  astro_sphere_sink.cpp:190-191
      flux = sumFlux*k (f32),  area = sumArea = D_p        (f32, **不量化**)
  profile=1 (write_hips_phase1)  astro_sphere_sink.cpp:520-542
      S = clamp(D_p/a_cell, 0, 1);  q = lround(255*S);
      flux = sumFlux*k (f32),  area = (q/255)*a_cell (f32) **量化到 a_cell/255 格点**
  writer finalize  aio_hips_writer.cpp:1322-1341
      covered = valid & area>0 & finite ;  sig = flux/area ;  sup = area/A_cell (>1 钳)
      未覆盖 -> support=0 且 signal=NaN

  k = D_p/N_p（astro_sphere_sink.h:50-52），两末端同一函数，故 flux 相同。
  => sig1/sig0 = D_p/A_cov，A_cov = (q/255)*a_cell
  => 相对偏差 r(S) = 255*S/q - 1，|255S - q| <= 0.5 => |r| <= 0.5/(255S-0.5) ~ 1/(510 S)
  => NaN 触发：q==0 <=> 255S < 0.5 <=> S < 1/510 = 1.9608e-3
  => variance 偏差 = (D_p/A_cov)^2 = (1+r)^2-1 ~ 2r（幂次 +2 的直接推论）

本脚本：(1) 我自己造一组 support 分布（含成稿未取的格点中点、跨 1/510 门限两侧、
S>1 钳制侧、S=1）在 double 与 **float32 产品位深**两档下逐点算 r；
(2) 与包络 1/(510S) 对比；(3) 验 2 倍律；(4) 枚举 q==0 的触发边界。
"""
import json
import math
import os

import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "w3_out.json")
R = {}


def lround_half_away(x):
    """C lround: 半数远离零取整（与 std::lround 一致）。"""
    return math.floor(x + 0.5) if x >= 0 else math.ceil(x - 0.5)


def quantize(S, a_cell, dtype=np.float32):
    """复现 astro_sphere_sink.cpp:520-542 的 profile=1 通道。"""
    Sc = min(max(S, 0.0), 1.0)
    q = lround_half_away(255.0 * Sc)
    q = max(0, min(255, q))
    area_f32 = dtype((q / 255.0) * a_cell)
    return q, float(area_f32), (q == 0)


def both_ends(D_p, a_cell, F_p, N_p, dtype=np.float32):
    """两个直写末端各自发布的 signal/variance（writer 归约后）。"""
    k = (D_p / N_p) if N_p > 0.0 else 1.0        # sb_publish_scale
    flux = dtype(F_p * k)
    # profile=0
    area0 = dtype(D_p)
    cov0 = area0 > 0.0 and math.isfinite(area0)
    sig0 = float(flux / area0) if cov0 else float("nan")
    sup0 = float(min(area0 / a_cell, 1.0)) if cov0 else 0.0
    clamp0 = (area0 / a_cell) > 1.0
    # profile=1
    q, area1, zero = quantize(D_p / a_cell, a_cell, dtype)
    cov1 = (not zero) and area1 > 0.0
    sig1 = float(flux / area1) if cov1 else float("nan")
    sup1 = 0.0 if zero else float(min(area1 / a_cell, 1.0))
    return {"q": q, "support_true": D_p / a_cell, "sig_profile0": sig0,
            "sig_profile1": sig1, "rel_dev_sig": (sig1 / sig0 - 1.0)
            if (cov0 and cov1 and sig0 != 0) else None,
            "nan_profile1": bool(not cov1), "nan_profile0": bool(not cov0),
            "support_published_p1": sup1, "support_published_p0": sup0,
            "writer_clamped_p0": bool(clamp0)}


def main():
    N = 512                                        # 生产最小 nside（sink 要求 nside>=512）
    a_cell = 4.0 * math.pi / (12.0 * N * N)
    N_p = 0.83 * a_cell                            # 任取的归一分母（pf<1 典型）
    F_p = 1.7                                      # 任取分配通量
    res = {}

    # (1) 我自己选的 support 档：格点中点（最坏）、格点上（最好）、门限两侧、S>1、S=1
    supports = {
        "S=1.0 (满覆盖整格)": 1.0,
        "S=0.64 (pf=0.8 典型)": 0.64,
        "S=254.5/255 (1 下方中点, 最坏)": 254.5 / 255.0,
        "S=163.5/255 (中点)": 163.5 / 255.0,
        "S=128/255 (整格)": 128.0 / 255.0,
        "S=0.05": 0.05,
        "S=12.5/255 (低档中点)": 12.5 / 255.0,
        "S=0.01": 0.01,
        "S=2.5/255 (刚过 NaN 门限)": 2.5 / 255.0,
        "S=1.5/255 (仍 >0.5/255)": 1.5 / 255.0,
        "S=0.5/255 (恰半数, lround 远离零 -> 1)": 0.5 / 255.0,
        "S=0.49/255 (低于半数 => q=0)": 0.49 / 255.0,
        "S=1.30 (过覆盖, 钳到 1)": 1.30,
    }
    rows = {}
    for tag, S in supports.items():
        D_p = S * a_cell
        d64 = both_ends(D_p, a_cell, F_p, N_p, np.float64)
        d32 = both_ends(D_p, a_cell, F_p, N_p, np.float32)
        env = (1.0 / (510.0 * S)) if S > 0 else None
        var_ratio = (d32["rel_dev_sig"] and
                     ((1.0 + d32["rel_dev_sig"]) ** 2 - 1.0))
        rows[tag] = {"S": S, "q": d64["q"],
                     "rel_dev_double": d64["rel_dev_sig"],
                     "rel_dev_f32_product": d32["rel_dev_sig"],
                     "envelope_1_over_510S": env,
                     "variance_rel_dev_from_same_area": var_ratio,
                     "nan_p1": d64["nan_profile1"], "nan_p0": d64["nan_profile0"],
                     "support_pub_p1": d64["support_published_p1"],
                     "support_pub_p0": d64["support_published_p0"],
                     "writer_clamp_p0_fires": d64["writer_clamped_p0"]}
    res["selected_supports"] = rows

    # (2) 全 256 档 + 全中点扫描：最坏残差 vs 包络；以及 f32 位深带来的额外项
    worst_env_gap = 0.0
    n_nan = 0
    scan = []
    for q in range(0, 256):
        for frac, kind in ((0.0, "grid"), (0.5, "midpoint")):
            S = (q + frac) / 255.0
            if S > 1.0:
                continue
            D_p = S * a_cell
            d = both_ends(D_p, a_cell, F_p, N_p, np.float64)
            r = d["rel_dev_sig"]
            if d["nan_profile1"]:
                n_nan += 1
                scan.append({"q": q, "kind": kind, "S": S, "nan": True})
                continue
            if r is None:
                continue
            env = 1.0 / (510.0 * S)
            worst_env_gap = max(worst_env_gap, abs(r) - env if S > 1e-9 else 0.0)
            scan.append({"q": q, "kind": kind, "S": S, "rel_dev": r, "env": env})
    res["scan"] = {
        "n_cases": len(scan),
        "n_nan_cases": n_nan,
        "max_abs_rel_dev": max(abs(x.get("rel_dev", 0.0)) for x in scan),
        "max_abs_rel_dev_S_gt_0.05": max(abs(x["rel_dev"]) for x in scan
                                         if x.get("rel_dev") is not None and x["S"] > 0.05),
        "worst_envelope_violation_double": worst_env_gap,
        "nan_first_S": min((x["S"] for x in scan if x.get("nan")), default=None),
        "nan_threshold_exact": 0.5 / 255.0,
        "q1_S_range_for_nan": "0 <= 255S < 0.5  <=> S < 1/510",
    }
    # 逐 q 的包络实测（double 与 f32 两档，看位深是否额外放大）
    per_q = {}
    for q in (1, 2, 3, 13, 64, 128, 163, 200, 254, 255):
        S = q / 255.0
        D_p = S * a_cell
        dd = both_ends(D_p, a_cell, F_p, N_p, np.float64)
        df = both_ends(D_p, a_cell, F_p, N_p, np.float32)
        # 最坏情形：同一 q 档的区间中点
        Sm = (q + 0.5) / 255.0 if q < 255 else 1.0
        dm = both_ends(Sm * a_cell, a_cell, F_p, N_p, np.float64)
        per_q["q%d" % q] = {"S": S, "rel_dev_gridpoint_d": dd["rel_dev_sig"],
                            "rel_dev_gridpoint_f32": df["rel_dev_sig"],
                            "rel_dev_midpoint_d": dm["rel_dev_sig"],
                            "S_true_at_midpoint": Sm,
                            "envelope": (1.0 / (510.0 * S) if S > 0 else None)}
    res["per_q"] = per_q

    # (3) FP32 累加噪声的量级对照（成稿说差 4 个数量级，我自己算一个界）
    #     产品 signal 位深 f32: eps=2^-24=5.96e-8；累加 N 项的最坏相对 ~ sqrt(N)*eps
    eps = 2.0 ** -24
    res["fp32_floor"] = {"eps_f32": eps,
                         "worst_sum_rel_1024_terms": float(math.sqrt(1024) * eps),
                         "quantum_over_fp32_floor_q64": float(
                             (0.5 / 64.0) / (math.sqrt(1024) * eps))}

    # (4) 与 pixfrac 的关系：满覆盖时 D_p = ?（pf<1 时典型 support）
    #     drizzle_engine.cpp:1617 weight=a/A_drop, :1640 sumArea+=overlap_area
    #     满覆盖叶: D_p = sum_j a_jp ≈ A_cell * (覆盖比例)。pf 不直接进 D_p, 但 pf<1
    #     时相邻源像素的 drop 之间有空隙 => 单叶 D_p 可远小于 A_cell => 低 support 档被用到。
    res["pf_note"] = {
        "k_equals": "D_p/N_p (= pf^2 严格当 A_drop=pf^2*A_pixel)",
        "sig0_equals": "F_p/N_p = S_p (精确, 与 pf 无关)",
        "sig1_equals": "S_p * D_p/A_cov (A_cov 量化 => 偏差 1/(510 S))",
    }
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(res, fh, ensure_ascii=False, indent=1)
    print(json.dumps(res["selected_supports"], ensure_ascii=False, indent=1))
    print(json.dumps(res["scan"], indent=1))
    print(json.dumps(res["per_q"], indent=1))
    print(json.dumps(res["fp32_floor"], indent=1))


if __name__ == "__main__":
    main()
