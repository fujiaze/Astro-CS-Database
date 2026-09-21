import sys, json, itertools, time, os
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C

OUT = 'run/RELEASE-02/q3-additive-truth/data'
os.makedirs(OUT, exist_ok=True)

def frame_sky(frame):
    d = C.frame_data(frame)
    a = np.asarray(d[::16, ::16], dtype=np.float64)
    return float(np.median(a))

def pair_stats(fA, fB, snr_min=50.0):
    t0 = time.time()
    m = C.match_stars(fA, fB, snr_min=snr_min, rad_arcsec=3.0)
    n0 = len(m['xA'])
    if n0 < 25:
        return None
    T = C.fit_transform(m, deg=1)
    if T is not None:
        xr, yr = C.apply_transform(T, m['xA'], m['yA'])
        k = np.hypot(xr - m['xB'], yr - m['yB']) < 0.7
        m = {kk: (np.asarray(vv)[k] if isinstance(vv, np.ndarray) and np.asarray(vv).shape[:1] == k.shape else vv)
             for kk, vv in m.items()}
    fa = np.asarray(m['fA'], float); fb = np.asarray(m['fB'], float)
    r = fa / fb
    g = np.isfinite(r) & (r > 0)
    r = r[g]; xa = np.asarray(m['xA'])[g]; ya = np.asarray(m['yA'])[g]
    if len(r) < 25:
        return None
    lr = np.log(r)
    med = float(np.median(r)); mad = float(1.4826 * np.median(np.abs(lr - np.median(lr))))
    # spatial linear model of log r
    u_ = (xa - 2048.0) / 2048.0; v_ = (ya - 2048.0) / 2048.0
    M = np.vstack([np.ones_like(u_), u_, v_]).T
    sol, *_ = np.linalg.lstsq(M, lr, rcond=None)
    res = lr - M @ sol
    return dict(n=n0, n_used=int(len(r)), med=med, logmad=mad,
                s_u=float(sol[1]), s_v=float(sol[2]),
                res_logr=float(np.std(res)),
                t=time.time() - t0)

if __name__ == '__main__':
    reg = q3lib.registry()
    sky = {}
    for f in reg:
        sky[f['stem']] = frame_sky(f)
    print('sky medians:', {k[:28]: round(v, 1) for k, v in sky.items()}, flush=True)
    out = []
    pairs = list(itertools.combinations(reg, 2))
    t00 = time.time()
    for i, (fA, fB) in enumerate(pairs):
        st = pair_stats(fA, fB)
        if st is None:
            continue
        same_grp = fA['group'] == fB['group']
        same_panel = fA['panel'] == fB['panel']
        rec = dict(A=fA['stem'], B=fB['stem'], grpA=fA['group'], grpB=fB['group'],
                   panelA=fA['panel'], panelB=fB['panel'], telA=fA['tel'], telB=fB['tel'],
                   same_grp=same_grp, same_panel=same_panel,
                   skyA=sky[fA['stem']], skyB=sky[fB['stem']], **st)
        out.append(rec)
        if i % 50 == 0:
            print('pair %d/%d  %.0fs elapsed' % (i, len(pairs), time.time() - t00), flush=True)
    json.dump(dict(sky=sky, pairs=out), open(OUT + '/pairs_star.json', 'w'), indent=1)
    print('DONE', len(out), 'pairs', time.time() - t00, flush=True)
