import sys, json, itertools
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C

def star_ratio(fA, fB, snr_min=50.0, rad=3.0, refine=True):
    m = C.match_stars(fA, fB, snr_min=snr_min, rad_arcsec=rad)
    if refine and len(m['xA']) > 30:
        T = C.fit_transform(m, deg=1)
        if T is not None:
            xr, yr = C.apply_transform(T, m['xA'], m['yA'])
            k = np.hypot(xr - m['xB'], yr - m['yB']) < 0.7
            m = {kk: (np.asarray(vv)[k] if isinstance(vv, np.ndarray) and np.asarray(vv).shape[:1] == k.shape else vv)
                 for kk, vv in m.items()}
    fa = np.asarray(m['fA'], float); fb = np.asarray(m['fB'], float)
    r = fa / fb
    good = np.isfinite(r) & (r > 0) & (fa > 0) & (fb > 0)
    rr = r[good]
    med = float(np.median(rr)); mad = float(1.4826 * np.median(np.abs(rr - med)))
    m = {kk: (np.asarray(vv)[good] if isinstance(vv, np.ndarray) and np.asarray(vv).shape[:1] == good.shape else vv)
         for kk, vv in m.items()}
    return dict(m=m, r=rr, med=med, mad=mad, n=int(good.sum()))

def report(grp, reg):
    g = [x for x in reg if x['group'] == grp]
    print('=' * 110); print(grp)
    for fA, fB in itertools.combinations(g, 2):
        d = star_ratio(fA, fB)
        m = d['m']; r = d['r']
        line = '%s vs %s n=%4d  med(fA/fB)=%.5f MAD=%.4f' % (
            fA['stem'][:30], fB['stem'][:30], d['n'], d['med'], d['mad'])
        R = np.hypot(np.asarray(m['xA']) - 2048, np.asarray(m['yA']) - 2048)
        qs = np.percentile(R, [0, 20, 40, 60, 80, 100])
        parts = []
        for i in range(5):
            k = (R >= qs[i]) & (R <= qs[i + 1])
            if k.sum() > 10: parts.append('%.4f' % np.median(r[k]))
        line += '  | radial med(r): ' + ' '.join(parts)
        print(line)
        fq = np.percentile(np.asarray(m['fA']), [0, 20, 40, 60, 80, 100])
        parts = []
        for i in range(5):
            k = (np.asarray(m['fA']) >= fq[i]) & (np.asarray(m['fA']) <= fq[i + 1])
            if k.sum() > 10: parts.append('%.4f' % np.median(r[k]))
        print('        flux-binned med(r): ' + ' '.join(parts))
        xa = np.asarray(m['xA']); ya = np.asarray(m['yA'])
        print('        x<2048 %.4f  x>=2048 %.4f | y<2048 %.4f  y>=2048 %.4f' % (
            np.median(r[xa < 2048]), np.median(r[xa >= 2048]),
            np.median(r[ya < 2048]), np.median(r[ya >= 2048])))

if __name__ == '__main__':
    reg = q3lib.registry()
    for grp in ['t2_m1_red', 't2_m2_red', 't3_m1_red', 't2_m3_red', 't2_m5_red']:
        report(grp, reg)
