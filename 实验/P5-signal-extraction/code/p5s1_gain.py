"""第 1 步：逐帧逐半径带 s_k ± se —— 像素级局地差分估计量。

为什么换估计量：上一轮用粗格（57.6″）均值的比值，被「格内结构采样差异 + 共格集合随帧对变化」污染，
估出 0.009…38.7（无效）。正确做法是在**同一批天球位置**上取**帧内局地差分**：
加性偏置与平滑天光面在局地差分里精确消掉，剩下的幅度比就是两帧的相对增益。

实现（一次遍历即可，内存只需 43 × 423k 采样点）：
  ① 在生产网格上按步长 8 抽样（8×1.8″ = 14.4″ 的差分臂长），把每个抽样位置映射到帧像素坐标
     （帧→输出的射影矩阵取逆），双线性取样 ⇒ 每帧一张抽样图 S_k；
  ② 对每帧在抽样图上取相邻差分 Δ_k = S_k[:, :-1] − S_k[:, 1:]（x 方向）与 y 方向；
  ③ 对每对帧 (i,j)、每个半径带 b： s_ij(b) = Σ Δi Δj / Σ Δj²，se 由残差用 δ 法给出；
  ④ 按 log s_ij = log g_i − log g_j（规范 Σ_i log g_i = 0，按 1/se² 加权）解逐帧、逐带 g_k ± se；
  ⑤ 对照：k_photo（P1 产物）、帧天光电平、同一指向（M1…M6）子样本（同指向帧的天光完全相同，
     差分里只剩乘性/平场残差，是最干净的子样本）。
"""

from __future__ import annotations

import glob
import json
import os
import re
import sys

import numpy as np
from astropy.io import fits

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5z_run as Z                      # noqa: E402

PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
STRIDE = 8                 # 输出网格抽样步长（8 × 1.8″ = 14.4″）
NB = 6                     # 半径带数


def out_samples():
    """生产网格上的抽样位置（0 基输出像素坐标）。"""
    xs = np.arange(0, Z.NOUT, STRIDE)
    ys = np.arange(0, Z.POUT, STRIDE)
    XX, YY = np.meshgrid(xs, ys)
    return XX.ravel().astype(np.float64), YY.ravel().astype(np.float64)


def frac_v(p):
    return ((p for p in p),)


def frame_sample(fw, dw, img, bad, H, xs, ys):
    """把帧影像按 WCS 双线性取样到输出抽样位置。H 为帧像素→输出像素的射影矩阵。"""
    Hi = np.linalg.inv(H)
    q = np.concatenate([xs[:, None], ys[:, None], np.ones((xs.size, 1))], axis=1) @ Hi.T
    xf = q[:, 0] / q[:, 2]
    yf = q[:, 1] / q[:, 2]
    ix = np.floor(xf); iy = np.floor(yf)
    ok = (ix >= 0) & (ix < img.shape[1] - 1) & (iy >= 0) & (iy < img.shape[0] - 1)
    ix = np.clip(ix, 0, img.shape[1] - 2).astype(np.int64)
    iy = np.clip(iy, 0, img.shape[0] - 2).astype(np.int64)
    fx = xf - ix; fy = yf - iy
    a = img[iy, ix]; b = img[iy, ix + 1]; c = img[iy + 1, ix]; d = img[iy + 1, ix + 1]
    v = ((1 - fx) * (1 - fy) * a + fx * (1 - fy) * b +
         (1 - fx) * fy * c + fx * fy * d)
    badm = (bad[iy, ix] > 0) | (bad[iy, ix + 1] > 0) | (bad[iy + 1, ix] > 0) | (bad[iy + 1, ix + 1] > 0)
    good = ok & np.isfinite(v) & ~badm
    v = np.where(good, v, np.nan)
    return v


def local_diff(S, nx):
    """抽样图 → x/y 方向局地差分（与抽样位置掩膜）。"""
    A = S.reshape(-1, nx)
    dx = A[:, :-1] - A[:, 1:]
    dy = A[:-1, :] - A[1:, :]
    return dx, dy


def radius_band_of(nx, bands):
    xs = np.arange(0, Z.NOUT, STRIDE); ys = np.arange(0, Z.POUT, STRIDE)
    XX, YY = np.meshgrid(xs, ys)
    du = (XX - (Z.NOUT - 1) / 2.0) * Z.CD_DEG
    dv = (YY - (Z.POUT - 1) / 2.0) * Z.CD_DEG
    R = np.hypot(du, dv)
    b = np.digitize(R, bands[1:-1])
    bx = b[:, :-1]                                   # x 差分用左端点的带
    by = b[:-1, :]
    return bx.ravel(), by.ravel(), R


def main():
    dw, dh = Z.ref_grid()
    xs, ys = out_samples()
    nx = xs.size // len(np.unique(ys)) if False else np.unique(xs).size
    ny = np.unique(ys).size
    frames = []
    for tag in ("t2", "t3"):
        man = json.load(open(os.path.join(PDIR, tag, "p1_products.json")))
        for e in man["frames"]:
            stem = os.path.basename(e["input_light"]).replace(".fts", "")
            path = os.path.join(PDIR, tag, f"cleaned_{stem}.fts")
            badp = os.path.join(PDIR, tag, f"badcol_{stem}.fts")
            if not os.path.exists(path):
                continue
            try:
                fw, fh = Z.frame_wcs(path); H = Z._homography(fw, dw)
            except Exception as exc:
                print("  跳过（无几何）:", stem, type(exc).__name__); continue
            frames.append({"tag": tag, "stem": stem, "path": path, "bad": badp,
                           "pointing": (re.search(r"_M(\d)_", stem) or [None, "?"])[1],
                           "band": tag})
    print("帧数（有几何）:", len(frames))
    kp = {}
    for tag in ("t2", "t3"):
        d = json.load(open(os.path.join(PDIR, tag, "p1_phot.json")))
        for k, v in d["photscale_detail"].items():
            kp[k] = v["k_photo"]
    S = np.full((len(frames), xs.size), np.nan, dtype=np.float32)
    kphot = np.full(len(frames), np.nan)
    levels = np.full(len(frames), np.nan)
    for k, fr in enumerate(frames):
        img = fits.getdata(fr["path"]).astype(np.float64)
        bad = fits.getdata(fr["bad"]) if os.path.exists(fr["bad"]) else np.zeros_like(img)
        fw, _ = Z.frame_wcs(fr["path"]); H = Z._homography(fw, dw)
        S[k] = frame_sample(fw, dw, img, bad, H, xs, ys).astype(np.float32)
        kphot[k] = kp.get(fr["stem"], np.nan)
        levels[k] = np.nanmedian(S[k])
        if k % 6 == 0:
            print("  取样 %2d/%d" % (k + 1, len(frames)), flush=True)
    np.save(os.path.join(OUT, "p5s1_S.npy"), S)
    np.save(os.path.join(OUT, "p5s1_kphot.npy"), kphot)
    D = {"nx": int(nx), "ny": int(ny), "stride": STRIDE,
         "frames": [{"stem": f["stem"], "pointing": f["pointing"], "tag": f["tag"]} for f in frames]}
    rmax = float(np.hypot(((Z.NOUT - 1) / 2.0) * Z.CD_DEG, ((Z.POUT - 1) / 2.0) * Z.CD_DEG))
    bands = np.linspace(0, rmax, NB + 1)
    D["bands_deg"] = bands.tolist()
    bx, by, R = radius_band_of(nx, bands)
    DX = np.full((len(frames), bx.size), np.nan, dtype=np.float32)
    DY = np.full((len(frames), by.size), np.nan, dtype=np.float32)
    for k in range(len(frames)):
        dxk, dyk = local_diff(S[k], nx)
        DX[k] = dxk.ravel(); DY[k] = dyk.ravel()
    nf = len(frames)
    # 逐对、逐带统计（x 与 y 合并）
    out = np.full((nf, NB), np.nan); outse = np.full((nf, NB), np.nan)
    Dan = np.concatenate([DX, DY], axis=1)
    Ban = np.concatenate([bx, by])
    rows, cols, vals, rhs, ws, info = [], [], [], [], [], []
    r = 0
    for i in range(nf):
        for j in range(i + 1, nf):
            for b in range(NB):
                m = (Ban == b) & np.isfinite(Dan[i]) & np.isfinite(Dan[j])
                n = int(m.sum())
                if n < 200:
                    continue
                xi = Dan[i][m]; xj = Dan[j][m]
                den = float(np.sum(xj * xj))
                if den <= 0:
                    continue
                s = float(np.sum(xi * xj) / den)
                if not np.isfinite(s) or s <= 0.05 or s > 20:
                    continue
                resid = xi - s * xj
                se = float(np.sqrt(np.sum(resid ** 2) / max(n - 1, 1) / den))
                if not np.isfinite(se) or se <= 0:
                    continue
                w = 1.0 / (se / s) ** 2
                rows.append(r); cols.append((b, i)); vals.append(1.0); ws.append(w)
                rows.append(r); cols.append((b, j)); vals.append(-1.0); ws.append(w)
                rhs.append(np.log(s)); r += 1
                info.append({"b": b, "i": i, "j": j, "s": s, "se": se, "n": n,
                             "same_pointing": frames[i]["pointing"] == frames[j]["pointing"]})
    A = np.zeros((r, NB * nf)); y = np.zeros(r); W = np.zeros(r)
    for t in range(r):
        (b, i), (b2, j) = cols[2 * t], cols[2 * t + 1]
        A[t, b * nf + i] = vals[2 * t]; A[t, b * nf + j] = vals[2 * t + 1]
        y[t] = rhs[t]; W[t] = ws[2 * t]
    G = np.zeros((NB, NB * nf))
    for b in range(NB):
        for i in range(nf):
            G[b, b * nf + i] = 1.0
    Aw = np.vstack([A * np.sqrt(W)[:, None], G * 1e-3]); yw = np.concatenate([y * np.sqrt(W), np.zeros(NB)])
    x, *_ = np.linalg.lstsq(Aw, yw, rcond=None)
    res = Aw @ x - yw
    dof = max(r - NB * nf + NB, 1)
    s2 = float(np.sum(res ** 2) / dof)
    cov = s2 * np.linalg.pinv(Aw.T @ Aw)
    for b in range(NB):
        for i in range(nf):
            out[i, b] = np.exp(x[b * nf + i])
            outse[i, b] = out[i, b] * np.sqrt(max(cov[b * nf + i, b * nf + i], 0))
    # 同指向子样本单独解一次（天光完全相同的帧对：最干净的乘性读数）
    def solve_subset(sel):
        rr = [t for t in range(r) if sel(info[t])]
        if len(rr) < 5:
            return None
        A2 = np.zeros((len(rr), NB * nf)); y2 = np.zeros(len(rr)); W2 = np.zeros(len(rr))
        for t, tt in enumerate(rr):
            d = info[tt]
            A2[t, d["b"] * nf + d["i"]] = 1.0; A2[t, d["b"] * nf + d["j"]] = -1.0
            y2[t] = np.log(d["s"]); W2[t] = 1.0 / (d["se"] / d["s"]) ** 2
        Aw2 = np.vstack([A2 * np.sqrt(W2)[:, None], G * 1e-3])
        yy2 = np.concatenate([y2 * np.sqrt(W2), np.zeros(NB)])
        xx, *_ = np.linalg.lstsq(Aw2, yy2, rcond=None)
        g = np.full((nf, NB), np.nan)
        for b in range(NB):
            for i in range(nf):
                g[i, b] = np.exp(xx[b * nf + i])
        return g
    same = solve_subset(lambda d: d["same_pointing"])
    D.update({"n_pairs_bands": r, "g_per_band": out.tolist(), "se_per_band": outse.tolist(),
              "g_same_pointing": None if same is None else same.tolist(),
              "k_photo": kphot.tolist(), "sample_median_level": levels.tolist(),
              "band_edges_deg": bands.tolist()})
    json.dump(D, open(os.path.join(OUT, "p5s1_gains.json"), "w"), indent=1)
    gm = np.median(out, axis=1)
    print("解出 %d 对×带统计（%d 条方程）" % (r, r))
    print("逐帧 g_k（各带中位）: 中位 %.5f  范围 %.5f–%.5f" % (np.median(gm), np.nanmin(gm), np.nanmax(gm)))
    print("带间极差中位 %.5f（半径依赖）" % np.median(np.nanmax(out, axis=1) - np.nanmin(out, axis=1)))
    if same is not None:
        print("同指向子样本 g_k 范围 %.5f–%.5f" % (np.nanmin(same), np.nanmax(same)))
    print("写入", os.path.join(OUT, "p5s1_gains.json"))


if __name__ == "__main__":
    main()
