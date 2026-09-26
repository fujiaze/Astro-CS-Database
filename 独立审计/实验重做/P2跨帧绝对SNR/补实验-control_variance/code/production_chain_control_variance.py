#!/usr/bin/env python3
"""生产链忠实臂：逐句复刻 sampler.cpp:837-878 的 control 观测生成链（不含 k_corr，置 1）：
  m0 = median(vals)（偶数 N 取两中央序统计量均值，同 median_of）
  s0 = 1.482602218505602 * MAD(vals; m0)
  亮端迭代裁剪 iters=3, clip_sigma=3.0：
    nr = {v <= m0 + 3*s0}；|nr| < min_samples(5) => break（ret 不变）
    nm = median(nr)；|nm - m0| < 1e-12*max(|m0|,1e-12) => ret=nr, break（s0 不变）
    否则 m0=nm, ret=nr, s1 = 1.4826*MAD(ret; m0)，s1<=0 => break（s0 不变），s0=s1
  y = m0；sigma = s0；n_retained = |ret|；cvar = (pi/2) * sigma^2 / n_retained
精度量：ratio_B2 = E[cvar] / Var(y)（control_variance 声称的被估量 = control estimator
(patch median) 的统计方差）。
固定 seed：20260605。运行：python3 production_chain_control_variance.py"""
import json
import math
import os

import numpy as np

K_PI_HALF = math.pi / 2.0
K_MAD = 1.482602218505602
CLIP_SIGMA = 3.0
CLIP_ITERS = 3
MIN_SAMPLES = 5
SEED = 20260605
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "production_chain_control_variance.json")


def med_even(a_sorted, nk):
    """a_sorted: (R, N) 升序，非保留位已置 +inf；nk: (R,) 保留数。
    返回保留集的 median（偶数取两中央均值）。"""
    R = a_sorted.shape[0]
    lo = (nk - 1) // 2
    hi = nk // 2
    rows = np.arange(R)
    return 0.5 * (a_sorted[rows, lo] + a_sorted[rows, hi])


def mad_of(a_sorted, nk, center):
    """保留集的 1.4826*MAD(center)。非保留位 dev 置 +inf 后排序取中位。"""
    R, N = a_sorted.shape
    dev = np.abs(a_sorted - center[:, None])
    dev_sorted = np.sort(dev, axis=1)
    return K_MAD * med_even(dev_sorted, nk)


def chain(x):
    """x: (m, N) 原始 patch。返回 y, sigma, n_ret（逐 rep）。"""
    R, N = x.shape
    xs = np.sort(x, axis=1)
    m0 = med_even(xs, np.full(R, N))
    s0 = mad_of(xs, np.full(R, N), m0)
    keep = np.ones_like(x, dtype=bool)
    nk = np.full(R, N)
    for _ in range(CLIP_ITERS):
        thr = m0 + CLIP_SIGMA * s0
        new_keep = keep & (x <= thr[:, None])
        new_nk = new_keep.sum(axis=1)
        # |nr| < min_samples => break（ret/nk 不变）
        active = new_nk >= MIN_SAMPLES
        if not active.any():
            break
        tmp = np.where(new_keep, x, np.inf)
        tmp_sorted = np.sort(tmp, axis=1)
        nm = med_even(tmp_sorted, new_nk)
        conv = np.abs(nm - m0) < 1e-12 * np.maximum(np.abs(m0), 1e-12)
        # 收敛者：ret=nr（nk 更新），s0/m0 不变，跳出
        frozen = active & conv
        # 推进者：m0=nm, ret=nr, s1 更新
        adv = active & ~conv
        if frozen.any():
            nk = np.where(frozen, new_nk, nk)
            keep = np.where(frozen[:, None], new_keep, keep)
        if adv.any():
            m0 = np.where(adv, nm, m0)
            nk = np.where(adv, new_nk, nk)
            keep = np.where(adv[:, None], new_keep, keep)
            s1 = np.where(adv, mad_of(tmp_sorted, new_nk, m0), s0)
            bad = adv & (s1 <= 0.0)          # s1<=0 => break，s0 不变（ret/nk 已推进）
            s0 = np.where(adv & ~bad, s1, s0)
        if not adv.any():
            break
    return m0, s0, nk


def run(n, R, rng):
    ch = int(min(200_000, max(10_000, 4.0e7 / n)))
    ys = np.empty(R); ss = np.empty(R); nks = np.empty(R, dtype=np.int64)
    done = 0
    while done < R:
        m = min(ch, R - done)
        x = rng.standard_normal((m, n))
        y, s, nk = chain(x)
        ys[done:done+m] = y; ss[done:done+m] = s; nks[done:done+m] = nk
        done += m
    cvar = K_PI_HALF * ss ** 2 / nks
    var_y = ys.var(ddof=1)
    clip_frac = float((nks < R * 0 + n).mean())
    return {"N": n, "R": R,
            "kappa_y": var_y * n,                 # Var(y)*N（y 的真方差，N 归一）
            "var_y": var_y,
            "mean_cvar_over_var_y": float(cvar.mean() / var_y),
            "c_mad2_retained": float((ss ** 2).mean()),
            "mean_n_retained_over_N": float(nks.mean() / n),
            "clip_frac_any": clip_frac,
            "sigma_over_true": float(np.sqrt((ss ** 2).mean()))}


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "clip_sigma": CLIP_SIGMA, "clip_iters": CLIP_ITERS,
           "min_samples": MIN_SAMPLES, "arms": []}
    for n, R in [(5, 2_000_000), (9, 2_000_000), (17, 2_000_000), (20, 2_000_000),
                 (33, 1_000_000), (65, 1_000_000), (129, 400_000), (289, 400_000)]:
        row = run(n, R, rng)
        res["arms"].append(row)
        print(f"N={n:4d} R={R}: Var(y)*N={row['kappa_y']:.4f}  "
              f"E[cvar]/Var(y)={row['mean_cvar_over_var_y']:+.4f}  "
              f"E[sigma]/1={row['sigma_over_true']:.4f}  "
              f"E[n_ret]/N={row['mean_n_retained_over_N']:.4f}  "
              f"clip_frac={row['clip_frac_any']:.4f}")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print("written:", OUT)


if __name__ == "__main__":
    main()
