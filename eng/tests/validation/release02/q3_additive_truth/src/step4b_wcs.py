"""All-pairs star ratios using the effective SIP WCS directly (no empirical
affine), which is required for the narrow-overlap adjacent-panel (seam) pairs.
sep cut 1.5" is validated by the cross-panel separation histogram."""
import sys, json, itertools, time, os
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C

OUT = 'run/RELEASE-02/q3-additive-truth/data'
os.makedirs(OUT, exist_ok=True)
CUT = 1.5

def pair_wcs(fA, fB, snr_min=50.0):
    m = C.match_stars(fA, fB, snr_min=snr_min, rad_arcsec=3.0, wcsf=C.wcs_sip)
    sep = np.asarray(m['sep_arcsec'])
    k = sep < CUT
    m = {kk: (np.asarray(vv)[k] if isinstance(vv, np.ndarray) and np.asarray(vv).shape[:1] == k.shape else vv)
         for kk, vv in m.items()}
    n = len(m['xA'])
    if n < 20:
        return None
    fa = np.asarray(m['fA'], float); fb = np.asarray(m['fB'], float)
    r = fa / fb
    g = np.isfinite(r) & (r > 0)
    r = r[g]; xa = np.asarray(m['xA'])[g]; ya = np.asarray(m['yA'])[g]
    lr = np.log(r)
    u_ = (xa - 2048.) / 2048.; v_ = (ya - 2048.) / 2048.
    R = np.hypot(u_, v_)
    M1 = np.vstack([np.ones_like(u_), u_, v_]).T
    M2 = np.vstack([np.ones_like(u_), u_, v_, R ** 2]).T
    s1, *_ = np.linalg.lstsq(M1, lr, rcond=None)
    s2, *_ = np.linalg.lstsq(M2, lr, rcond=None)
    return dict(n=int(n), n_used=int(len(r)), med=float(np.median(r)),
                logmad=float(1.4826 * np.median(np.abs(lr - np.median(lr)))),
                s_u=float(s1[1]), s_v=float(s1[2]), r2c=float(s2[3]),
                rms0=float(np.std(lr)), rms1=float(np.std(lr - M1 @ s1)),
                rms2=float(np.std(lr - M2 @ s2)),
                xA_lo=float(xa.min()), xA_hi=float(xa.max()),
                yA_lo=float(ya.min()), yA_hi=float(ya.max()))

if __name__ == '__main__':
    reg = q3lib.registry()
    out = []
    pairs = list(itertools.combinations(reg, 2))
    t0 = time.time()
    for i, (fA, fB) in enumerate(pairs):
        st = pair_wcs(fA, fB)
        if st is None:
            continue
        same_grp = fA['group'] == fB['group']
        same_panel = fA['panel'] == fB['panel']
        adj = (not same_panel) and abs(int(fA['panel'][1]) - int(fB['panel'][1])) == 1
        out.append(dict(A=fA['stem'], B=fB['stem'], grpA=fA['group'], grpB=fB['group'],
                        panelA=fA['panel'], panelB=fB['panel'], telA=fA['tel'], telB=fB['tel'],
                        same_grp=same_grp, same_panel=same_panel, adjacent=adj, **st))
        if i % 200 == 0:
            print('pair %d/%d %.0fs' % (i, len(pairs), time.time() - t0), flush=True)
    json.dump(out, open(OUT + '/pairs_wcs.json', 'w'), indent=1)
    print('DONE', len(out), time.time() - t0, flush=True)
