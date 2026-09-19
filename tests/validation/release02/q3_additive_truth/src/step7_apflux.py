"""Independent aperture photometry on the native calibrated frames (bypasses
the p1_flux catalog).  Local sky from an annulus; aperture r<=4 px."""
import sys, os, json, itertools
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C

OUT = 'run/RELEASE-02/q3-additive-truth/data'
os.makedirs(OUT, exist_ok=True)
R_AP = 4.0; R_IN = 8.0; R_OUT = 14.0
_H = 20  # half box

def ap_flux(img, xs, ys):
    H = _H
    n = len(xs)
    yy, xx = np.mgrid[-H:H + 1, -H:H + 1]
    rr = np.hypot(xx, yy)
    ap = rr <= R_AP
    ann = (rr >= R_IN) & (rr <= R_OUT)
    nap = ap.sum(); nann = ann.sum()
    out = np.full(n, np.nan)
    for i in range(n):
        x0 = int(round(xs[i])); y0 = int(round(ys[i]))
        if x0 < H or y0 < H or x0 >= 4096 - H or y0 >= 4096 - H:
            continue
        cut = np.asarray(img[y0 - H:y0 + H + 1, x0 - H:x0 + H + 1], dtype=np.float64)
        if not np.all(np.isfinite(cut)):
            continue
        sky = np.median(cut[ann])
        out[i] = np.sum(cut[ap] - sky)
    return out

def pair_apflux(fA, fB, snr_min=50.0):
    m = C.match_stars(fA, fB, snr_min=snr_min, rad_arcsec=3.0)
    T = C.fit_transform(m, deg=1)
    xr, yr = C.apply_transform(T, m['xA'], m['yA'])
    k = np.hypot(xr - m['xB'], yr - m['yB']) < 0.7
    m = {kk: (np.asarray(vv)[k] if isinstance(vv, np.ndarray) and np.asarray(vv).shape[:1] == k.shape else vv)
         for kk, vv in m.items()}
    da = C.frame_data(fA); db = C.frame_data(fB)
    fa = ap_flux(da, m['xA'], m['yA'])
    fb = ap_flux(db, m['xB'], m['yB'])
    g = np.isfinite(fa) & np.isfinite(fb) & (fa > 0) & (fb > 0)
    r = fa[g] / fb[g]
    lr = np.log(r)
    xa = np.asarray(m['xA'])[g]; ya = np.asarray(m['yA'])[g]
    u_ = (xa - 2048.) / 2048.; v_ = (ya - 2048.) / 2048.
    M = np.vstack([np.ones_like(u_), u_, v_]).T
    sol, *_ = np.linalg.lstsq(M, lr, rcond=None)
    # radial term
    R = np.hypot(xa - 2048, ya - 2048) / 2048.
    M2 = np.vstack([np.ones_like(u_), u_, v_, R ** 2]).T
    sol2, *_ = np.linalg.lstsq(M2, lr, rcond=None)
    return dict(n=int(g.sum()), med=float(np.median(r)),
                logmad=float(1.4826 * np.median(np.abs(lr - np.median(lr)))),
                su=float(sol[1]), sv=float(sol[2]),
                r2coef=float(sol2[3]),
                rms0=float(np.std(lr)), rms1=float(np.std(lr - M @ sol)),
                rms2=float(np.std(lr - M2 @ sol2)),
                fa_med=float(np.median(fa[g])), fb_med=float(np.median(fb[g])),
                xa=xa.tolist()[:0], )

if __name__ == '__main__':
    reg = q3lib.registry()
    def find(p, t, d):
        return [x for x in reg if x['panel'] == p and x['tel'] == t and x['date'] == d][0]
    cases = [('M1', 'T2', '20251212', 'M1', 'T2', '20251224'),
             ('M2', 'T2', '20251212', 'M2', 'T2', '20251224'),
             ('M2', 'T2', '20251212', 'M2', 'T2', '20251216'),
             ('M1', 'T3', '20251126', 'M1', 'T3', '20251127'),
             ('M1', 'T3', '20251126', 'M1', 'T3', '20251213'),
             ('M1', 'T2', '20251212', 'M1', 'T3', '20251126'),
             ('M2', 'T2', '20251212', 'M2', 'T3', '20251128'),
             ('M5', 'T2', '20251212', 'M5', 'T2', '20251224')]
    res = {}
    for ca, cb in [(c[:3], c[3:]) for c in cases]:
        fA = find(*ca); fB = find(*cb)
        r = pair_apflux(fA, fB)
        tag = '%s_%s_%s|%s_%s_%s' % (ca + cb)
        res[tag] = r
        print('%-34s n=%5d med=%.5f mad=%.4f | su=%+.4f sv=%+.4f r2=%+.4f | rms0=%.4f rms1=%.4f rms2=%.4f' % (
            tag, r['n'], r['med'], r['logmad'], r['su'], r['sv'], r['r2coef'], r['rms0'], r['rms1'], r['rms2']), flush=True)
    json.dump(res, open(OUT + '/apflux.json', 'w'), indent=1)
    print('WROTE')
