#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-SPATIAL-GAIN ③b 像素级复核: 把拟合出的 m 真正乘到帧像素上, 重算帧间乘性差.

与 real_gain.py 的区别:
  - 本脚本**独立**在 calibrated_*.fts 上做 r=6px / 环 10-16px 固定孔径测光
    (与 p1_flux.json 的 r=4px 口径不同 => 同时是口径敏感性检查);
  - 真的把 m_A(x,y) / m_B(x,y) 乘到像素上再重测 (验证"施加 m"的语义);
  - 联合拟合在本脚本内自带 Tikhonov 先验 (tau_spatial), 不依赖 real_gain.json 里
    未加先验的系数 (那批系数在近零空间方向不可辨识, 见 real_ridge.py 说明)。

度量: 重叠区 8x8 分箱中位数的峰峰值; before = 仅帧级标量; after = 标量 + 低阶 m。

用法: python3 real_pixel_check.py [--pairs 6]
输出: ../data/real_pixel_check.json
"""
import os, sys, json, math, argparse, time
import numpy as np
from astropy.io import fits

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..", "..", "..", "synthetic")))
import real_gain as R  # noqa: E402

DATA = os.path.abspath(os.path.join(HERE, "..", "data"))
TAUS = [0.02, 0.005, 0.001]


def cal_path(F):
    # 标定帧落在 tile 根目录: norm/<tile>/calibrated_<base>.fts
    b = os.path.basename(F.file).replace('cleaned_', 'calibrated_')
    return os.path.join(R.NORM, F.tile, b)


def aperture_sum(img, xs, ys, r=6.0, rin=10.0, rout=16.0, chunk=3000):
    H, W = img.shape
    n = len(xs); Rr = int(math.ceil(rout))
    gy, gx = np.mgrid[-Rr:Rr + 1, -Rr:Rr + 1]
    d2 = gx.astype(float) ** 2 + gy.astype(float) ** 2
    ap = d2 <= r * r; an = (d2 >= rin * rin) & (d2 <= rout * rout)
    flux = np.full(n, np.nan)
    for s in range(0, n, chunk):
        e = min(n, s + chunk)
        x0 = np.floor(xs[s:e]).astype(int); y0 = np.floor(ys[s:e]).astype(int)
        ix = x0[:, None, None] + gx[None]; iy = y0[:, None, None] + gy[None]
        ok = (ix >= 0) & (ix < W) & (iy >= 0) & (iy < H)
        patch = np.where(ok, img[np.clip(iy, 0, H - 1), np.clip(ix, 0, W - 1)].astype(np.float64), np.nan)
        with np.errstate(all='ignore'):
            sk = np.nanmedian(np.where(an[None], patch, np.nan), axis=(1, 2))
            fl = np.nansum(np.where(ap[None], patch, np.nan) - sk[:, None, None], axis=(1, 2))
        good = np.isfinite(fl) & np.isfinite(sk) & (np.sum(ap & ok, axis=(1, 2)) > 10)
        idx = np.arange(s, e)
        flux[idx[good]] = fl[good]
    return flux


def binned_ptp(x, y, z, nb=8, minn=4):
    if len(x) < nb * nb * minn:
        return float('nan'), None
    xb = np.linspace(x.min(), x.max(), nb + 1); yb = np.linspace(y.min(), y.max(), nb + 1)
    xi = np.clip(np.digitize(x, xb) - 1, 0, nb - 1); yi = np.clip(np.digitize(y, yb) - 1, 0, nb - 1)
    M = np.full((nb, nb), np.nan)
    for a in range(nb):
        for b in range(nb):
            m = (xi == a) & (yi == b)
            if m.sum() >= minn:
                M[a, b] = np.median(z[m])
    v = M[np.isfinite(M)]
    if v.size < 4:
        return float('nan'), M
    return float(v.max() - v.min()), M


def pick_pairs(res, n_pairs):
    want = ["cross-tel", "same-tile", "adjacent-panel"]
    out = []
    for kind in want:
        cand = [r for r in res['pair_metrics'] if r['kind'] == kind and r['n'] >= 150]
        cand.sort(key=lambda r: -r['n'])
        out.extend(cand[:max(1, n_pairs // 3)])
    return out[:n_pairs]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pairs", type=int, default=6)
    args = ap.parse_args()
    t0 = time.time()
    print("[load] ..."); sys.stdout.flush()
    frames = R.load_all(verbose=False)
    pairs = R.build_pairs(frames, verbose=False)
    by_label = {F.label: F for F in frames}
    idx_of = {F.label: k for k, F in enumerate(frames)}
    print("  %d frames %d pairs (%.0fs)" % (len(frames), len(pairs), time.time() - t0)); sys.stdout.flush()

    fits_ = {}
    for tau in TAUS:
        for o in (1, 2):
            c, info, _ = R.joint_fit(frames, pairs, o, tau_spatial=tau, tau_const=0.5)
            fits_[(tau, o)] = c
            print("  fit tau=%-6s order=%d sigma=%.5f (%.0fs)" % (str(tau), o, info['sigma_dex'], time.time() - t0))
            sys.stdout.flush()

    sel = pick_pairs(json.load(open(os.path.join(DATA, "real_gain.json"))), args.pairs)
    out = []
    for r in sel:
        A = by_label[r['labelA']]; B = by_label[r['labelB']]
        i, j = R.match_pair(A, B)
        if len(i) < 100:
            continue
        imgA = fits.getdata(cal_path(A)).astype(np.float32)
        imgB = fits.getdata(cal_path(B)).astype(np.float32)
        fA = aperture_sum(imgA, A.x[i], A.y[i]); fB = aperture_sum(imgB, B.x[j], B.y[j])
        ok = np.isfinite(fA) & np.isfinite(fB) & (fA > 0) & (fB > 0)
        i2, j2, fA, fB = i[ok], j[ok], fA[ok], fB[ok]
        z = np.log10(fA / fB)
        xm = 0.5 * (A.x[i2] + B.x[j2]); ym = 0.5 * (A.y[i2] + B.y[j2])
        rec = {"kind": r['kind'], "labelA": A.label, "labelB": B.label, "n": int(len(z)),
               "z_median_mag": float(-2.5 * np.median(z)), "orders": {}}
        z0 = z - np.median(z)
        p0, _ = binned_ptp(xm, ym, z0)
        rec["before"] = dict(ptp8x8_pct=float((10 ** p0 - 1) * 100), ptp8x8_mag=float(2.5 * p0))
        for tau in TAUS:
            for o in (1, 2):
                c = fits_[(tau, o)]
                mdl = (R.surf_at(A, c[idx_of[A.label]], o, A.x[i2], A.y[i2])
                       - R.surf_at(B, c[idx_of[B.label]], o, B.x[j2], B.y[j2]))
                zc = z - mdl; zc = zc - np.median(zc)
                p, _ = binned_ptp(xm, ym, zc)
                rec["orders"]["tau%s_o%d" % (tau, o)] = dict(
                    ptp8x8_pct=float((10 ** p - 1) * 100), ptp8x8_mag=float(2.5 * p))
        # ---- 真正乘像素: A 帧乘 10^-surf_A, B 帧乘 10^-surf_B (tau=0.005, order=1) ----
        tau, o = 0.005, 1
        c = fits_[(tau, o)]
        yy, xx = np.mgrid[0:imgA.shape[0], 0:imgA.shape[1]].astype(np.float64)
        for img, F in ((imgA, A), (imgB, B)):
            s = R.surf_at(F, c[idx_of[F.label]], o, xx.ravel(), yy.ravel()).reshape(xx.shape)
            img *= np.power(10.0, -s).astype(np.float32)
        gA = aperture_sum(imgA, A.x[i2], A.y[i2]); gB = aperture_sum(imgB, B.x[j2], B.y[j2])
        ok2 = np.isfinite(gA) & np.isfinite(gB) & (gA > 0) & (gB > 0)
        z2 = np.log10(gA[ok2] / gB[ok2]); z2 = z2 - np.median(z2)
        p2, _ = binned_ptp(xm[ok2], ym[ok2], z2)
        rec["pixel_applied"] = dict(tau=tau, order=o, n=int(ok2.sum()),
                                    ptp8x8_pct=float((10 ** p2 - 1) * 100), ptp8x8_mag=float(2.5 * p2))
        del imgA, imgB
        out.append(rec)
        print("  %-16s %-26s x %-26s n=%4d before=%6.3f%% o1(tau.005)=%6.3f%% pixel-applied=%6.3f%%" % (
            rec['kind'], A.label[:26], B.label[:26], rec['n'], rec['before']['ptp8x8_pct'],
            rec['orders']['tau0.005_o1']['ptp8x8_pct'], rec['pixel_applied']['ptp8x8_pct']))
        sys.stdout.flush()

    with open(os.path.join(DATA, "real_pixel_check.json"), "w") as fh:
        json.dump({"pairs": out, "taus": TAUS, "elapsed_s": time.time() - t0}, fh, indent=1)
    print("[done] %.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
