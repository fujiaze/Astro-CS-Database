"""Detailed pair analysis: star-based spatial multiplicative structure +
pixel-level alpha with additive-field control + counterfactual normalisations."""
import sys, json, os
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C, q3hp

OUT = 'run/RELEASE-02/q3-additive-truth/data'
os.makedirs(OUT, exist_ok=True)

def refined_match(fA, fB, snr=50.0):
    m = C.match_stars(fA, fB, snr_min=snr, rad_arcsec=3.0)
    T = C.fit_transform(m, deg=1)
    if T is None: return None, None
    xr, yr = C.apply_transform(T, m['xA'], m['yA'])
    k = np.hypot(xr - m['xB'], yr - m['yB']) < 0.7
    m = {kk: (np.asarray(vv)[k] if isinstance(vv, np.ndarray) and np.asarray(vv).shape[:1] == k.shape else vv)
         for kk, vv in m.items()}
    T2 = C.fit_transform(m, deg=1)
    return T2, m

def poly_terms(u, v, deg):
    ts = [np.ones_like(u)]
    for p in range(deg + 1):
        for q in range(deg + 1 - p):
            if p + q == 0: continue
            ts.append(u ** p * v ** q)
    return np.vstack(ts).T

def star_spatial(m, blocks=8):
    """Block-wise star ratio map + polynomial fits deg0/1/2 + significance."""
    fa = np.asarray(m['fA'], float); fb = np.asarray(m['fB'], float)
    xa = np.asarray(m['xA'], float); ya = np.asarray(m['yA'], float)
    r = fa / fb
    g = np.isfinite(r) & (r > 0)
    r = r[g]; xa = xa[g]; ya = ya[g]
    lr = np.log(r)
    u_ = (xa - 2048.) / 2048.; v_ = (ya - 2048.) / 2048.
    out = {}
    for deg in [0, 1, 2]:
        M = poly_terms(u_, v_, deg)
        sol, *_ = np.linalg.lstsq(M, lr, rcond=None)
        res = lr - M @ sol
        out['rms_deg%d' % deg] = float(np.std(res))
        out['coef_deg%d' % deg] = sol.tolist()
    # amplitude of spatial surface (deg2 minus mean), in percent
    M2 = poly_terms(u_, v_, 2)
    s2 = out['coef_deg2']
    surf = M2 @ np.array(s2)
    # evaluate on corners
    uc = np.array([-1, 1, -1, 1, 0.]); vc = np.array([-1, -1, 1, 1, 0.])
    sc = poly_terms(uc, vc, 2) @ np.array(s2)
    out['spatial_p2p_pct'] = float((np.exp(sc.max()) - np.exp(sc.min())) * 100)
    out['spatial_std_pct'] = float(np.std(np.exp(surf) - 1) * 100)
    # block map
    bx = np.clip((xa / 4096 * blocks).astype(int), 0, blocks - 1)
    by = np.clip((ya / 4096 * blocks).astype(int), 0, blocks - 1)
    bmap = np.full((blocks, blocks), np.nan); bcnt = np.zeros((blocks, blocks), int)
    for i in range(blocks):
        for j in range(blocks):
            k = (bx == i) & (by == j)
            bcnt[i, j] = k.sum()
            if k.sum() > 5: bmap[i, j] = float(np.median(r[k]))
    out['block_map'] = bmap.tolist(); out['block_cnt'] = bcnt.tolist()
    out['med'] = float(np.median(r)); out['n'] = int(len(r))
    out['logmad'] = float(1.4826 * np.median(np.abs(lr - np.median(lr))))
    return out

def analyse_pair(fA, fB, step=24, snr=50.0, tag=''):
    T, m = refined_match(fA, fB, snr)
    if T is None or len(m['xA']) < 50:
        return None
    sp = star_spatial(m)
    a_scalar = sp['med']
    # spatial star model (deg 2) for the counterfactual
    fa = np.asarray(m['fA'], float); fb = np.asarray(m['fB'], float)
    rr = fa / fb; gg = np.isfinite(rr) & (rr > 0)
    uu = (np.asarray(m['xA'], float)[gg] - 2048.) / 2048.
    vv = (np.asarray(m['yA'], float)[gg] - 2048.) / 2048.
    M2 = poly_terms(uu, vv, 2)
    coef2, *_ = np.linalg.lstsq(M2, np.log(rr[gg]), rcond=None)
    S = C.sample_pair(fA, fB, T, step=step)
    vA, vB = S['vA'], S['vB']
    d0 = vA - vB; L0 = 0.5 * (vA + vB)
    res = {}
    res['raw'] = q3hp.hp_alpha(S['xA'], S['yA'], d0, L0, win=256)
    res['raw_win128'] = q3hp.hp_alpha(S['xA'], S['yA'], d0, L0, win=128)
    res['raw_win512'] = q3hp.hp_alpha(S['xA'], S['yA'], d0, L0, win=512)
    # scalar counterfactual: vA/a
    vA1 = vA / a_scalar
    d1 = vA1 - vB; L1 = 0.5 * (vA1 + vB)
    res['scalar'] = q3hp.hp_alpha(S['xA'], S['yA'], d1, L1, win=256)
    # spatial counterfactual: vA / a(p)
    uS = (S['xA'] - 2048.) / 2048.; vS = (S['yA'] - 2048.) / 2048.
    aS = np.exp(poly_terms(uS, vS, 2) @ coef2)
    vA2 = vA / aS
    d2 = vA2 - vB; L2 = 0.5 * (vA2 + vB)
    res['spatial'] = q3hp.hp_alpha(S['xA'], S['yA'], d2, L2, win=256)
    # residual spatial structure after scalar normalisation: block medians of d1
    bx = np.clip((S['xA'] / 4096 * 8).astype(int), 0, 7)
    by = np.clip((S['yA'] / 4096 * 8).astype(int), 0, 7)
    dmap = np.full((8, 8), np.nan)
    for i in range(8):
        for j in range(8):
            k = (bx == i) & (by == j)
            if k.sum() > 20: dmap[i, j] = float(np.median(d1[k]))
    res['dmap_scalar'] = dmap.tolist()
    dmap0 = np.full((8, 8), np.nan)
    for i in range(8):
        for j in range(8):
            k = (bx == i) & (by == j)
            if k.sum() > 20: dmap0[i, j] = float(np.median(d0[k]))
    res['dmap_raw'] = dmap0.tolist()
    res['star'] = sp
    res['n_samp'] = int(len(vA))
    res['medA'] = float(np.median(vA)); res['medB'] = float(np.median(vB))
    res['tag'] = tag
    res['A'] = fA['stem']; res['B'] = fB['stem']
    return res

if __name__ == '__main__':
    reg = q3lib.registry()
    def find(panel, tel, date):
        c = [x for x in reg if x['panel'] == panel and x['tel'] == tel and x['date'] == date]
        assert c, (panel, tel, date)
        return c[0]
    cases = [
        ('M1', 'T2', '20251212', 'M1', 'T2', '20251224'),
        ('M2', 'T2', '20251212', 'M2', 'T2', '20251224'),
        ('M2', 'T2', '20251212', 'M2', 'T2', '20251216'),
        ('M1', 'T3', '20251126', 'M1', 'T3', '20251127'),
        ('M1', 'T3', '20251126', 'M1', 'T3', '20251213'),
        ('M1', 'T2', '20251212', 'M1', 'T3', '20251126'),
        ('M2', 'T2', '20251212', 'M2', 'T3', '20251128'),
        ('M1', 'T2', '20251212', 'M2', 'T2', '20251212'),
        ('M2', 'T2', '20251212', 'M3', 'T2', '20251216'),
        ('M5', 'T2', '20251212', 'M6', 'T2', '20251212'),
    ]
    out = {}
    for ca, cb in [(c[:3], c[3:]) for c in cases]:
        fA = find(*ca); fB = find(*cb)
        tag = '%s_%s_%s|%s_%s_%s' % (ca + cb)
        r = analyse_pair(fA, fB, tag=tag)
        if r is None:
            print('skip', ca, cb); continue
        out[tag] = r
        s = r['star']
        print('%-24s n=%5d med=%.5f mad=%.4f | spatial p2p=%.2f%% rms0=%.4f rms1=%.4f rms2=%.4f | hpA raw=%+.5f(w128 %+.5f w512 %+.5f) scalar=%+.5f spatial=%+.5f' % (
            r['tag'], s['n'], s['med'], s['logmad'], s['spatial_p2p_pct'],
            s['rms_deg0'], s['rms_deg1'], s['rms_deg2'],
            r['raw']['alpha'], r['raw_win128']['alpha'], r['raw_win512']['alpha'],
            r['scalar']['alpha'], r['spatial']['alpha']), flush=True)
    json.dump(out, open(OUT + '/pair_detail.json', 'w'), indent=1)
    print('WROTE', OUT + '/pair_detail.json')
