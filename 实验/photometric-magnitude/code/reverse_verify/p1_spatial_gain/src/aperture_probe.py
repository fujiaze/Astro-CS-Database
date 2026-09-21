#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-SPATIAL-GAIN ③e 孔径探针: 空间乘法增益是否与孔径无关?

动机: real_pixel_check.py 在 **r=6/环 10-16** 的独立测光上复核时, 用 **r=4/环 6-10**
(p1_flux.json 口径) 拟合出的 m 反而把 3x3 峰峰做大了。若残差是真正的**乘法增益**
(平场), 则它与孔径无关: 用任何孔径测得的帧间比值都应被同一个 m 吃掉。
本脚本对同一批帧对, 在 r = 3,4,6,10 上分别测光, 报:
  before  = 逐星 log10(fA/fB) 的 3x3 分箱峰峰 (仅扣帧级标量)
  after   = 再除以 r=4 拟合出的 m 比值后的 3x3 分箱峰峰
若 after 在各孔径下都显著小于 before ⇒ 是乘法增益; 若 after 只在 r=4 上小 ⇒ 是孔径伪影。

用法: python3 aperture_probe.py [--pairs 3]
输出: ../data/aperture_probe.json
"""
import os, sys, json, math, argparse, time
import numpy as np
from astropy.io import fits

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..", "..", "..", "synthetic")))
import real_gain as R          # noqa: E402
from real_pixel_check import aperture_sum, binned_ptp, cal_path  # noqa: E402

DATA = os.path.abspath(os.path.join(HERE, "..", "data"))
RADII = [(3.0, 6.0, 10.0), (4.0, 6.0, 10.0), (6.0, 10.0, 16.0), (10.0, 16.0, 24.0)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pairs", type=int, default=3)
    ap.add_argument("--tau", type=float, default=0.005)
    args = ap.parse_args()
    t0 = time.time()
    frames = R.load_all(verbose=False)
    pairs = R.build_pairs(frames, verbose=False)
    by_label = {F.label: F for F in frames}
    idx_of = {F.label: k for k, F in enumerate(frames)}
    c, info, _ = R.joint_fit(frames, pairs, 1, tau_spatial=args.tau, tau_const=0.5)
    print("fit sigma=%.5f (%.0fs)" % (info['sigma_dex'], time.time() - t0)); sys.stdout.flush()

    res = json.load(open(os.path.join(DATA, "real_gain.json")))
    sel = []
    for kind in ("cross-tel", "same-tile"):
        cand = [r for r in res['pair_metrics'] if r['kind'] == kind and r['n'] >= 300]
        cand.sort(key=lambda r: -r['n'])
        sel.append(cand[0])
    sel = sel[:args.pairs]

    out = []
    for r0 in sel:
        A = by_label[r0['labelA']]; B = by_label[r0['labelB']]
        i, j = R.match_pair(A, B)
        imgA = fits.getdata(cal_path(A)).astype(np.float32)
        imgB = fits.getdata(cal_path(B)).astype(np.float32)
        rec = {"kind": r0['kind'], "labelA": A.label, "labelB": B.label, "n": int(len(i)), "radii": {}}
        for (rr, rin, rout) in RADII:
            fA = aperture_sum(imgA, A.x[i], A.y[i], rr, rin, rout)
            fB = aperture_sum(imgB, B.x[j], B.y[j], rr, rin, rout)
            ok = np.isfinite(fA) & np.isfinite(fB) & (fA > 0) & (fB > 0)
            i2, j2, fA2, fB2 = i[ok], j[ok], fA[ok], fB[ok]
            z = np.log10(fA2 / fB2)
            xm = 0.5 * (A.x[i2] + B.x[j2]); ym = 0.5 * (A.y[i2] + B.y[j2])
            z0 = z - np.median(z)
            p0, _, cnt = binned_ptp(xm, ym, z0, nb=3)
            sig = 1.482602218505602 * np.median(np.abs(z0 - np.median(z0)))
            mdl = (R.surf_at(A, c[idx_of[A.label]], 1, A.x[i2], A.y[i2])
                   - R.surf_at(B, c[idx_of[B.label]], 1, B.x[j2], B.y[j2]))
            zc = z - mdl; zc = zc - np.median(zc)
            p1, _, _ = binned_ptp(xm, ym, zc, nb=3)
            # 只扣 m 的**纯空间部分**(去均值), 即真正的"空间结构"
            d = mdl - np.median(mdl)
            p2, _, _ = binned_ptp(xm, ym, (z - d) - np.median(z - d), nb=3)
            rec["radii"]["r%.0f" % rr] = dict(
                n=int(ok.sum()), median_stars_per_bin=float(cnt),
                sigma_z_pct=float((10 ** float(sig) - 1) * 100),
                before_ptp3x3_pct=float((10 ** p0 - 1) * 100),
                after_ptp3x3_pct=float((10 ** p1 - 1) * 100),
                model_ptp_pct=float((10 ** float(d.max() - d.min()) - 1) * 100))
            print("  %-10s r=%4.1f n=%4d | before=%7.3f%%  after=%7.3f%%  |m_model pp|=%6.3f%%" % (
                r0['kind'], rr, ok.sum(), rec["radii"]["r%.0f" % rr]["before_ptp3x3_pct"],
                rec["radii"]["r%.0f" % rr]["after_ptp3x3_pct"], rec["radii"]["r%.0f" % rr]["model_ptp_pct"]))
            sys.stdout.flush()
        out.append(rec)
        del imgA, imgB

    with open(os.path.join(DATA, "aperture_probe.json"), "w") as fh:
        json.dump({"pairs": out, "tau": args.tau, "radii": RADII, "elapsed_s": time.time() - t0}, fh, indent=1)
    print("[done] %.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
