"""Final alpha measurement on native frames with the effective SIP WCS.

Sampling: grid in frame-A pixels -> sky (A SIP WCS) -> frame-B pixels (B SIP WCS).
Both frames are box-smoothed (win) so that PSF/seeing differences between the
two epochs cannot masquerade as a multiplicative signal-level term.
Criteria: d = vA - vB = alpha * L + beta(p);  beta(p) arbitrary smooth additive.
"""
import sys, json, os
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C, q3hp
from scipy.ndimage import uniform_filter
from astropy.wcs import WCS

OUT = 'run/RELEASE-02/q3-additive-truth/data'
os.makedirs(OUT, exist_ok=True)

def wcs_pair(fA, fB):
    return C.wcs_sip(fA), C.wcs_sip(fB)

def sample_wcs(fA, fB, step=24, win=8, border=16):
    WA, WB = wcs_pair(fA, fB)
    gx = np.arange(border, 4096 - border, step, dtype=float)
    GX, GY = np.meshgrid(gx, gx)
    GX = GX.ravel(); GY = GY.ravel()
    ra, dec = WA.all_pix2world(GX, GY, 0)
    XB, YB = WB.all_world2pix(ra, dec, 0)
    k = np.isfinite(XB) & np.isfinite(YB) & (XB > border) & (XB < 4096 - border) & (YB > border) & (YB < 4096 - border)
    GX, GY, XB, YB = GX[k], GY[k], XB[k], YB[k]
    da = np.asarray(C.frame_data(fA), dtype=np.float32)
    db = np.asarray(C.frame_data(fB), dtype=np.float32)
    if win > 1:
        da = uniform_filter(da, size=win, mode='nearest')
        db = uniform_filter(db, size=win, mode='nearest')
    va = q3lib.sample_bilinear(da, GX, GY)
    vb = q3lib.sample_bilinear(db, XB, YB)
    m = np.isfinite(va) & np.isfinite(vb)
    return GX[m], GY[m], va[m], vb[m]

def star_model(fA, fB, deg=2):
    m = C.match_stars(fA, fB, snr_min=50.0, rad_arcsec=3.0, wcsf=C.wcs_sip)
    k = np.asarray(m['sep_arcsec']) < 1.5
    m = {kk: (np.asarray(vv)[k] if isinstance(vv, np.ndarray) and np.asarray(vv).shape[:1] == k.shape else vv)
         for kk, vv in m.items()}
    fa = np.asarray(m['fA'], float); fb = np.asarray(m['fB'], float)
    r = fa / fb
    g = np.isfinite(r) & (r > 0)
    xa = np.asarray(m['xA'], float)[g]; ya = np.asarray(m['yA'], float)[g]; lr = np.log(r[g])
    u_ = (xa - 2048.) / 2048.; v_ = (ya - 2048.) / 2048.
    ts = [np.ones_like(u_)]
    for p in range(deg + 1):
        for q in range(deg + 1 - p):
            if p + q == 0: continue
            ts.append(u_ ** p * v_ ** q)
    M = np.vstack(ts).T
    sol, *_ = np.linalg.lstsq(M, lr, rcond=None)
    a0 = float(np.median(r[g]))
    rms1 = float(np.std(lr - M @ sol)); rms0 = float(np.std(lr - np.median(lr)))
    return dict(a_scalar=a0, coef=sol.tolist(), deg=deg, n=int(g.sum()),
                rms0=rms0, rms1=rms1, med=float(np.median(r[g])))

def alpha_of(fA, fB, step=24, win=8, sm=None):
    if sm is None:
        sm = star_model(fA, fB)
    X, Y, va, vb = sample_wcs(fA, fB, step=step, win=win)
    d0 = va - vb; L0 = 0.5 * (va + vb)
    out = {}
    out['n'] = int(len(va))
    out['raw'] = q3hp.hp_alpha(X, Y, d0, L0, win=256)['alpha']
    out['raw_w128'] = q3hp.hp_alpha(X, Y, d0, L0, win=128)['alpha']
    a = sm['a_scalar']
    va1 = va / a; d1 = va1 - vb
    out['scalar'] = q3hp.hp_alpha(X, Y, d1, 0.5 * (va1 + vb), win=256)['alpha']
    # spatial a(p) from the star model
    u_ = (X - 2048.) / 2048.; v_ = (Y - 2048.) / 2048.
    ts = [np.ones_like(u_)]
    for p in range(sm['deg'] + 1):
        for q in range(sm['deg'] + 1 - p):
            if p + q == 0: continue
            ts.append(u_ ** p * v_ ** q)
    aS = np.exp(np.vstack(ts).T @ np.array(sm['coef']))
    va2 = va / aS; d2 = va2 - vb
    out['spatial'] = q3hp.hp_alpha(X, Y, d2, 0.5 * (va2 + vb), win=256)['alpha']
    out['a_scalar'] = a
    out['a_star_minus1'] = a - 1.0
    out['a_star_minus1_spatial_rms'] = sm['rms1']
    out['medA'] = float(np.median(va)); out['medB'] = float(np.median(vb))
    return out

if __name__ == '__main__':
    reg = q3lib.registry()
    def find(p, t, d):
        return [x for x in reg if x['panel'] == p and x['tel'] == t and x['date'] == d][0]
    cases = [
        ('M1', 'T2', '20251212', 'M1', 'T2', '20251224', 'same-panel same-tel'),
        ('M2', 'T2', '20251212', 'M2', 'T2', '20251224', 'same-panel same-tel'),
        ('M5', 'T2', '20251212', 'M5', 'T2', '20251224', 'same-panel same-tel'),
        ('M1', 'T3', '20251126', 'M1', 'T3', '20251127', 'same-panel same-tel'),
        ('M1', 'T2', '20251212', 'M1', 'T3', '20251126', 'same-panel CROSS-tel'),
        ('M2', 'T2', '20251212', 'M2', 'T3', '20251128', 'same-panel CROSS-tel'),
        ('M1', 'T2', '20251212', 'M2', 'T2', '20251212', 'ADJACENT panel'),
        ('M2', 'T2', '20251212', 'M3', 'T2', '20251216', 'ADJACENT panel'),
        ('M5', 'T2', '20251212', 'M6', 'T2', '20251212', 'ADJACENT panel'),
    ]
    res = {}
    for ca, cb, tag in [(c[:3], c[3:6], c[6]) for c in cases]:
        fA = find(*ca); fB = find(*cb)
        try:
            sm = star_model(fA, fB)
            a = alpha_of(fA, fB, sm=sm)
        except Exception as e:
            print('FAIL', tag, e); continue
        key = '%s_%s_%s|%s_%s_%s' % (ca + cb)
        res[key] = dict(tag=tag, star=sm, alpha=a)
        print('%-24s %-26s nstar=%4d a=%.4f(a-1=%+.4f) spRMS=%.4f | alpha raw=%+.5f w128=%+.5f scalar=%+.5f spatial=%+.5f n=%d' % (
            tag, ca[0] + ca[1], sm['n'], sm['a_scalar'], sm['a_scalar'] - 1, sm['rms1'],
            a['raw'], a['raw_w128'], a['scalar'], a['spatial'], a['n']), flush=True)
    json.dump(res, open(OUT + '/alpha_final.json', 'w'), indent=1)
    print('WROTE')
