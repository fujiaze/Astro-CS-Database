#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1 ② 形式化：帧级标度散度是否由"逐帧星样本组成"（匹配星数/信噪/星等窗口）驱动。

判读（写死，不调参）：
  · 若统一样本定义后**散度塌到标准误量级** ⇒ ② 成立（散度由逐帧星样本组成驱动）；
  · 若仍远大于标准误 ⇒ ② 不足以结案。

做法（全部只用该帧自身星点；不动生产实现）：
  (0) 现状：location = 帧内 r 的 Tukey-IRLS 稳健位置（全 used 样本）
  (i) 统一星等/信噪窗口：只保留 SNR ∈ [lo,hi] 的星（lo/hi 取全体分位，不逐帧调）
  (ii) 统一样本量：每帧随机下采样到全体最小 n_used（固定种子 20260101）
  统计：帧间散度 SD / MAD÷0.6745、自助法标准误（帧内重采样）、散度/SE 倍数、
        帧级自助 95% CI（对帧重采样）。

只读 run/PHOT-FIX/**；输出 results/p1fix/kphoto_selection_test.json
"""
from __future__ import annotations
import json, math, re
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
MAD = 0.6744897501960817
C_TUKEY = 4.685
NBOOT = 200
SEED = 20260101


def tukey_location(r, iters=50, tol=1e-6):
    loc = float(np.median(r))
    for _ in range(iters):
        S = float(np.median(np.abs(r - loc)) / MAD)
        if S <= 0:
            break
        u = (r - loc) / (C_TUKEY * S)
        w = np.where(np.abs(u) < 1, (1 - u * u) ** 2, 0.0)
        if w.sum() <= 0:
            break
        new = float((w * r).sum() / w.sum())
        if abs(new - loc) < tol:
            return new
        loc = new
    return loc


def canon(s):
    return s.replace("cleaned_", "").replace(".fts", "").replace("@", "_")


def nn_index(src_xy, dst_xy, tol=5.0):
    out = np.full(len(dst_xy), -1, int)
    if len(src_xy) == 0:
        return out
    sx, sy = src_xy[:, 0], src_xy[:, 1]
    for i, (dx, dy) in enumerate(dst_xy):
        d2 = (sx - dx) ** 2 + (sy - dy) ** 2
        j = int(np.argmin(d2))
        if d2[j] <= tol * tol:
            out[i] = j
    return out


def rsigma(v):
    return float(np.median(np.abs(v - np.median(v))) / MAD)


def boot_ci(v, n=NBOOT, seed=SEED):
    rng = np.random.default_rng(seed)
    s = np.array([np.std(v[rng.integers(0, len(v), len(v))], ddof=1) for _ in range(n)])
    return float(np.percentile(s, 2.5)), float(np.percentile(s, 97.5))


def main():
    frames = []
    for tag in ("t2", "t3"):
        phot = json.loads((SRC / tag / "p1_phot.json").read_text())
        flux = json.loads((SRC / tag / "p1_flux.json").read_text())
        fmap = {canon(f["file"]): f for f in flux["frames"]}
        for fk in sorted(phot["spatial_gain"]):
            sg = phot["spatial_gain"][fk]; pd = phot["photscale_detail"][fk]
            s = sg.get("samples", {})
            if not s.get("x"):
                continue
            r = np.array(s["r_log10_finstr_over_fsyn"])
            X = np.array(s["x"]); Y = np.array(s["y"]); u = np.array(s["used"], bool)
            kp = pd.get("k_photo")
            if not (kp and kp > 0):
                continue
            snr = np.full(len(X), np.nan)
            if fk in fmap:
                R = fmap[fk]["results"]
                rxy = np.array([[q["x"], q["y"]] for q in R])
                for i, j in enumerate(nn_index(rxy, np.column_stack([X, Y]))):
                    if j >= 0:
                        v = R[j].get("snr")
                        if isinstance(v, (int, float)) and v == v:
                            snr[i] = v
            frames.append({"tag": tag, "frame": fk, "r": r, "x": X, "y": Y, "used": u,
                           "snr": snr, "loc_prod": -math.log10(kp),
                           "field_ptp": sg.get("field_ptp_dex") or 0.0})
    print("frames =", len(frames))

    def estimate(f, mode, snr_lo=None, snr_hi=None, cap=None, rng=None):
        m = f["used"].copy()
        if mode == "window":
            m &= np.isfinite(f["snr"]) & (f["snr"] >= snr_lo) & (f["snr"] <= snr_hi)
        elif mode == "downsample":
            idx = np.flatnonzero(m)
            if len(idx) > cap:
                m = np.zeros_like(m)
                m[rng.choice(idx, cap, replace=False)] = True
        if m.sum() < 30:
            return None, 0
        return tukey_location(f["r"][m]), int(m.sum())

    allsnr = np.concatenate([f["snr"][f["used"] & np.isfinite(f["snr"])] for f in frames])
    lo, hi = float(np.percentile(allsnr, 40)), float(np.percentile(allsnr, 60))
    print(f"统一 SNR 窗口 = [{lo:.1f}, {hi:.1f}]  (全体 40–60 分位, 不逐帧调)")

    variants = {}
    # (0) 现状
    variants["(0) 全样本(现状)"] = [estimate(f, "all")[0] for f in frames]
    # (i) 统一 SNR 窗口
    variants["(i) 统一SNR窗口"] = [estimate(f, "window", lo, hi)[0] for f in frames]
    # (ii) 统一样本量（下采样到全体最小 n_used）
    nmin = min(int(f["used"].sum()) for f in frames)
    rng = np.random.default_rng(SEED)
    variants[f"(ii) 下采样n={nmin}"] = [estimate(f, "downsample", cap=nmin, rng=rng)[0] for f in frames]
    print(f"全体最小 n_used = {nmin}")

    print()
    print("=== 各口径：帧间散度 与 散度/标准误 ===")
    res = {}
    for name, locs in variants.items():
        v = np.array([x for x in locs if x is not None], float)
        se = []
        for f, L in zip(frames, locs):
            if L is None:
                continue
            m = f["used"]
            rr = f["r"][m]
            rb = np.random.default_rng(SEED)
            se.append(np.std([tukey_location(rr[rb.integers(0, len(rr), len(rr))]) for _ in range(60)], ddof=1))
        se = np.array(se)
        sd = float(np.std(v, ddof=1)); rs = rsigma(v)
        cilo, cihi = boot_ci(v)
        res[name] = {"n": len(v), "sd": sd, "mad": rs, "se_med": float(np.median(se)),
                     "sd_over_se": sd / np.median(se), "mad_over_se": rs / np.median(se),
                     "sd_ci95": [cilo, cihi]}
        print("  %-18s n=%2d SD=%.6f [%.6f,%.6f] MAD=%.6f  SE中位=%.6f  ⇒ SD/SE=%.1f  MAD/SE=%.1f"
              % (name, len(v), sd, cilo, cihi, rs, np.median(se), sd / np.median(se), rs / np.median(se)))

    # 回归 |dev| ~ n_used
    from scipy import stats
    loc0 = np.array([x if x is not None else np.nan for x in variants["(0) 全样本(现状)"]], float)
    dev = np.abs(loc0 - np.nanmedian(loc0))
    nu = np.array([int(f["used"].sum()) for f in frames], float)
    ok = np.isfinite(dev) & np.isfinite(nu)
    lr = stats.linregress(nu[ok], dev[ok])
    print()
    print("=== 回归 |dev| ~ n_used ===")
    print("  slope=%.3e dex/星  r=%.3f  p=%.3g  n=%d  R²=%.3f"
          % (lr.slope, lr.rvalue, lr.pvalue, ok.sum(), lr.rvalue ** 2))
    resid = dev[ok] - (lr.intercept + lr.slope * nu[ok])
    print("  扣除 n_used 依赖后残余: SD=%.5f  MAD=%.5f dex  (原 |dev| MAD=%.5f)"
          % (np.std(resid, ddof=1), rsigma(resid), rsigma(dev[ok])))
    fp = np.array([f["field_ptp"] for f in frames], float)
    lr2 = stats.linregress(fp[ok], dev[ok])
    print("  （对照）|dev| ~ field_ptp: slope=%.4f r=%.3f p=%.3g" % (lr2.slope, lr2.rvalue, lr2.pvalue))

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "kphoto_selection_test.json").write_text(json.dumps(
        {"snr_window": [lo, hi], "n_min": nmin, "variants": res,
         "regression_n_used": {"slope": lr.slope, "r": lr.rvalue, "p": lr.pvalue, "r2": lr.rvalue ** 2},
         "resid_after_nused_sd": float(np.std(resid, ddof=1)),
         "resid_after_nused_mad": rsigma(resid)}, indent=1, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
