#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-SPATIAL-GAIN ③d 真实数据汇总表: 星覆盖 / 逐帧 m 形态 / before-after 分箱.

用法: python3 analyze_real.py
输出: ../data/real_analysis.json (+ stdout 表格, 供 report 引用)
"""
import os, sys, json, math
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import real_gain as R

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.abspath(os.path.join(HERE, "..", "data"))


def coverage(F, nb=3):
    xb = np.linspace(F.x.min(), F.x.max(), nb + 1)
    yb = np.linspace(F.y.min(), F.y.max(), nb + 1)
    xi = np.clip(np.digitize(F.x, xb) - 1, 0, nb - 1)
    yi = np.clip(np.digitize(F.y, yb) - 1, 0, nb - 1)
    C = np.zeros((nb, nb), int)
    for a in range(nb):
        for b in range(nb):
            C[a, b] = int(np.sum((xi == a) & (yi == b)))
    return C


def main():
    frames = R.load_all(verbose=False)
    by_label = {F.label: F for F in frames}
    out = {}

    # ---- 1. 星覆盖 ----
    cov = {}
    for F in frames:
        C = coverage(F, 3)
        C8 = coverage(F, 8)
        cov[F.label] = dict(tile=F.tile, idx=F.idx, n=int(len(F.x)),
                            blocks3x3_ge5=int(np.sum(C >= 5)), min_block3=int(C.min()),
                            blocks8x8_ge3=int(np.sum(C8 >= 3)), n8=int(C8.size),
                            x_range=[float(F.x.min()), float(F.x.max())],
                            y_range=[float(F.y.min()), float(F.y.max())])
    out['coverage'] = cov
    nb3 = np.array([v['blocks3x3_ge5'] for v in cov.values()])
    nb8 = np.array([v['blocks8x8_ge3'] for v in cov.values()])
    nst = np.array([v['n'] for v in cov.values()])
    out['coverage_summary'] = dict(
        n_star_median=int(np.median(nst)), n_star_min=int(nst.min()), n_star_max=int(nst.max()),
        blocks3x3_ge5_median=float(np.median(nb3)), blocks3x3_ge5_min=int(nb3.min()),
        frac_frames_full3x3=float(np.mean(nb3 == 9)),
        blocks8x8_ge3_median=float(np.median(nb8)), frac_blocks8x8_ge3_mean=float(np.mean(nb8 / 64)))
    print("[coverage] n_star med=%d min=%d max=%d | 3x3 blocks(>=5 stars) med=%.1f/9 min=%d, full=%d%% | 8x8 blocks(>=3) mean=%.0f%%" % (
        out['coverage_summary']['n_star_median'], out['coverage_summary']['n_star_min'],
        out['coverage_summary']['n_star_max'], out['coverage_summary']['blocks3x3_ge5_median'],
        out['coverage_summary']['blocks3x3_ge5_min'],
        100 * out['coverage_summary']['frac_frames_full3x3'],
        100 * out['coverage_summary']['frac_blocks8x8_ge3_mean']))

    # ---- 2. 逐帧 m (取 real_ridge.json 选定 tau) ----
    rj = json.load(open(os.path.join(DATA, "real_ridge.json")))
    scan = rj['scan']
    recs = [r for r in scan if r['tau_spatial'] == 0.02]
    per_frame = {}
    for r in recs:
        if r['order'] not in (1, 2):
            continue
        for lab, pp in zip(sorted(by_label), r['per_frame_m_pp']):
            per_frame.setdefault(lab, {})["order%d_pp_pct" % r['order']] = pp
    # 需要 coef 才能给梯度方向: 用 real_gain.json 里 order1 的 coef 不行(未加先验), 故只报幅度
    out['per_frame_m'] = per_frame
    pp1 = np.array([v['order1_pp_pct'] for v in per_frame.values()])
    pp2 = np.array([v['order2_pp_pct'] for v in per_frame.values()])
    out['m_amplitude'] = dict(
        order1=dict(median=float(np.median(pp1)), p10=float(np.percentile(pp1, 10)),
                    p90=float(np.percentile(pp1, 90)), max=float(pp1.max())),
        order2=dict(median=float(np.median(pp2)), p10=float(np.percentile(pp2, 10)),
                    p90=float(np.percentile(pp2, 90)), max=float(pp2.max())))
    print("[m amplitude] order1 pp: med=%.2f%% p10=%.2f p90=%.2f max=%.2f | order2 pp: med=%.2f%% p10=%.2f p90=%.2f max=%.2f" % (
        np.median(pp1), np.percentile(pp1, 10), np.percentile(pp1, 90), pp1.max(),
        np.median(pp2), np.percentile(pp2, 10), np.percentile(pp2, 90), pp2.max()))

    # ---- 3. before/after 按 n_star 分箱 ----
    before = rj['before']
    tbl = {}
    for r in recs:
        if r['order'] not in (1, 2):
            continue
        for key, arr in (("3x3", r['after_ptp3x3']), ("8x8", r['after_ptp8x8'])):
            for lo, hi in ((0, 100), (100, 200), (200, 400), (400, 10 ** 9)):
                sel = [i for i, b in enumerate(before) if lo <= b['n'] < hi]
                if len(sel) < 5:
                    continue
                b = np.array([before[i]['ptp3x3' if key == "3x3" else 'ptp8x8'] for i in sel], float)
                a = np.array([arr[i] for i in sel], float)
                ok = np.isfinite(b) & np.isfinite(a)
                if ok.sum() < 5:
                    continue
                tbl.setdefault(key, {})["o%d_n%d-%d" % (r['order'], lo, min(hi, 9999))] = dict(
                    n=int(ok.sum()), before=float(np.median(b[ok])), after=float(np.median(a[ok])),
                    frac_improved=float(np.mean(a[ok] < b[ok])),
                    median_ratio=float(np.median(a[ok] / b[ok])))
    out['by_nstar'] = tbl
    print("[by n_star] (median PTP %%, before -> after)")
    for key in ("3x3", "8x8"):
        for k in sorted(tbl.get(key, {})):
            v = tbl[key][k]
            print("   %-5s %-14s n=%3d  %7.3f%% -> %7.3f%%  (%.0f%% pairs improved, ratio %.2f)" % (
                key, k, v['n'], v['before'], v['after'], 100 * v['frac_improved'], v['median_ratio']))

    with open(os.path.join(DATA, "real_analysis.json"), "w") as fh:
        json.dump(out, fh, indent=1)
    print("[done]")


if __name__ == "__main__":
    main()
