#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-SPATIAL-GAIN ③c 真实数据: 先验收缩 (Tikhonov) 扫描 + 分半一致性.

背景 (关键可辨识性事实, 见 report):
  联合差分模型 z_AB = surf_A(p_A) - surf_B(p_B) 只能定出帧间**相对**空间增益。
  同一指向 (同一 tile) 下所有帧共有的像素空间图案在帧间差里完全抵消 ⇒ 近零空间,
  由噪声主导 ⇒ 逐帧绝对 m 的幅度不可辨识 (实测未加先验时 m pp 可达 100-3000%)。
  生产链每帧**独立**对 Gaia F_syn 拟合 (有绝对锚点), 不存在该退化 (合成实验已证)。
  本脚本用 Tikhonov 先验收缩 (空间系数 ~ N(0, tau^2), m -> 1) 做代理测量, 并给出
  分半一致性 (用一半星拟合的 m 与另一半比较) 作为"结构是真的还是过拟合"的判据。

用法: python3 real_ridge.py
输出: ../data/real_ridge.json
"""
import os, sys, json, math, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import real_gain as R
from gainlib import mad_sigma

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.abspath(os.path.join(HERE, "..", "data"))
TAUS = [None, 0.05, 0.02, 0.01, 0.005]
ORDERS = [0, 1, 2]


def m_stats(frames, coef, order):
    pp = []; rms = []
    for k, F in enumerate(frames):
        s = R.surf_frame(F, coef[k], order)
        pp.append(float((10 ** float(s.max() - s.min()) - 1) * 100))
        rms.append(float((10 ** float(np.std(s)) - 1) * 100))
    return np.array(pp), np.array(rms)


def grid_m(frames, coef, order, n=48):
    """逐帧在整帧网格上的 m (log10), 返回 {label: (X, Y, logm)}."""
    out = {}
    for k, F in enumerate(frames):
        x0, sx, y0, sy = R.frame_norm(F)
        gx = np.linspace(x0 - sx, x0 + sx, n); gy = np.linspace(y0 - sy, y0 + sy, n)
        GX, GY = np.meshgrid(gx, gy)
        s = R.surf_at(F, coef[k], order, GX.ravel(), GY.ravel()).reshape(GX.shape)
        out[F.label] = (GX, GY, s - s.mean())
    return out


def main():
    t0 = time.time()
    print("[load] ..."); sys.stdout.flush()
    frames = R.load_all(verbose=False)
    pairs = R.build_pairs(frames, verbose=False)
    print("  %d frames %d pairs (%.0fs)" % (len(frames), len(pairs), time.time() - t0)); sys.stdout.flush()

    # before 度量 (与 tau/order 无关): 每对 3x3 与 8x8 分箱 PTP
    before = []
    for p in pairs:
        A, B = frames[p['a']], frames[p['b']]
        z = np.log10(A.flux[p['i']] / B.flux[p['j']])
        xm = 0.5 * (A.x[p['i']] + B.x[p['j']]); ym = 0.5 * (A.y[p['i']] + B.y[p['j']])
        r0 = z - np.median(z)
        p3, _ = R.binned_ptp(xm, ym, r0, nb=3, minn=6)
        p8, _ = R.binned_ptp(xm, ym, r0, nb=8, minn=4)
        before.append(dict(kind=p['kind'], n=int(len(z)),
                           ptp3x3=float((10 ** p3 - 1) * 100) if np.isfinite(p3) else None,
                           ptp8x8=float((10 ** p8 - 1) * 100) if np.isfinite(p8) else None))
    print("[before] computed"); sys.stdout.flush()

    scan = []
    coefs = {}
    for tau in TAUS:
        for o in ORDERS:
            c, info, resid = R.joint_fit(frames, pairs, o, tau_spatial=tau, tau_const=0.5)
            pp, rms = m_stats(frames, c, o)
            rec = {"tau_spatial": tau, "order": o, "sigma_dex": info['sigma_dex'],
                   "n_outlier": info['n_outlier'],
                   "m_pp_pct": dict(median=float(np.median(pp)), p10=float(np.percentile(pp, 10)),
                                    p90=float(np.percentile(pp, 90)), max=float(pp.max())),
                   "m_rms_pct": dict(median=float(np.median(rms)), max=float(rms.max())),
                   "per_frame_m_pp": [float(v) for v in pp]}
            # after 度量
            aft3 = []; aft8 = []
            for pi, p in enumerate(pairs):
                A, B = frames[p['a']], frames[p['b']]
                z = np.log10(A.flux[p['i']] / B.flux[p['j']])
                mdl = (R.surf_at(A, c[p['a']], o, A.x[p['i']], A.y[p['i']])
                       - R.surf_at(B, c[p['b']], o, B.x[p['j']], B.y[p['j']]))
                r = z - mdl; r = r - np.median(r)
                xm = 0.5 * (A.x[p['i']] + B.x[p['j']]); ym = 0.5 * (A.y[p['i']] + B.y[p['j']])
                p3, _ = R.binned_ptp(xm, ym, r, nb=3, minn=6)
                p8, _ = R.binned_ptp(xm, ym, r, nb=8, minn=4)
                aft3.append(float((10 ** p3 - 1) * 100) if np.isfinite(p3) else np.nan)
                aft8.append(float((10 ** p8 - 1) * 100) if np.isfinite(p8) else np.nan)
            rec["after_ptp3x3"] = aft3; rec["after_ptp8x8"] = aft8
            for kind in ("same-tile", "adjacent-panel", "cross-tel", "ALL"):
                sel = [i for i, b in enumerate(before) if kind == "ALL" or b['kind'] == kind]
                if not sel:
                    continue
                b3 = np.array([before[i]['ptp3x3'] for i in sel], float)
                a3 = np.array([aft3[i] for i in sel], float)
                b8 = np.array([before[i]['ptp8x8'] for i in sel], float)
                a8 = np.array([aft8[i] for i in sel], float)
                ok = np.isfinite(b3) & np.isfinite(a3)
                rec["summary_" + kind] = dict(
                    n=int(ok.sum()),
                    before3=float(np.median(b3[ok])), after3=float(np.median(a3[ok])),
                    ratio3=float(np.median(a3[ok] / b3[ok])),
                    frac_improved3=float(np.mean(a3[ok] < b3[ok])),
                    before8=float(np.nanmedian(b8)), after8=float(np.nanmedian(a8)),
                    frac_improved8=float(np.nanmean(a8 < b8)))
            scan.append(rec)
            coefs[(tau, o)] = c
            s = rec["summary_ALL"]
            print("  tau=%-6s order=%d sigma=%.5f m_pp med=%7.3f%% max=%9.3f%% | 3x3 %.3f%%->%.3f%% (%.0f%% improved) | 8x8 %.3f%%->%.3f%%" % (
                str(tau), o, info['sigma_dex'], rec['m_pp_pct']['median'], rec['m_pp_pct']['max'],
                s['before3'], s['after3'], 100 * s['frac_improved3'], s['before8'], s['after8']))
            sys.stdout.flush()

    # ---- 分半一致性 (tau=0.02, order=1,2) ----
    rng = np.random.default_rng(4242)
    selA = {}; selB = {}
    for pi, p in enumerate(pairs):
        m = rng.random(len(p['i'])) < 0.5
        selA[pi] = m; selB[pi] = ~m
    cons = {}
    for o in (1, 2):
        cA, _, _ = R.joint_fit(frames, pairs, o, sel=selA, tau_spatial=0.02, tau_const=0.5)
        cB, _, _ = R.joint_fit(frames, pairs, o, sel=selB, tau_spatial=0.02, tau_const=0.5)
        gA = grid_m(frames, cA, o); gB = grid_m(frames, cB, o)
        rmsd = []; corr = []
        for lab in gA:
            a = gA[lab][2].ravel(); b = gB[lab][2].ravel()
            rmsd.append(float(np.sqrt(np.mean((a - b) ** 2))))
            corr.append(float(np.corrcoef(a, b)[0, 1]))
        cons[str(o)] = dict(rms_diff_dex=dict(median=float(np.median(rmsd)), max=float(np.max(rmsd))),
                            rms_diff_pct=float((10 ** float(np.median(rmsd)) - 1) * 100),
                            corr=dict(median=float(np.median(corr)), p10=float(np.percentile(corr, 10)),
                                      min=float(np.min(corr))))
        print("  [consistency] order=%d  halfA vs halfB: rms=%.4f dex (%.2f%%), corr median=%.3f min=%.3f" % (
            o, cons[str(o)]['rms_diff_dex']['median'], cons[str(o)]['rms_diff_pct'],
            cons[str(o)]['corr']['median'], cons[str(o)]['corr']['min']))
        sys.stdout.flush()
    # 与全样本 (tau=0.02) 的偏差
    dev = {}
    for o in (1, 2):
        cF = coefs[(0.02, o)]
        gF = grid_m(frames, cF, o)
        for tag, cc in (("halfA", None),):
            pass
        dev[str(o)] = dict(m_pp_median=float(np.median([np.ptp(gF[l][2]) for l in gF])))

    out = dict(taus=[str(t) for t in TAUS], orders=ORDERS, scan=scan,
               consistency=cons, before=before, elapsed_s=time.time() - t0,
               n_frames=len(frames), n_pairs=len(pairs))
    with open(os.path.join(DATA, "real_ridge.json"), "w") as fh:
        json.dump(out, fh, indent=1)
    print("[done] %.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
