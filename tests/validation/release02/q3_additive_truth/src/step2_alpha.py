import sys, json, itertools, time
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C

def refined_transform(fA, fB, snr=30.0, deg=1):
    m = C.match_stars(fA, fB, snr_min=snr, rad_arcsec=3.0)
    T = C.fit_transform(m, deg=1)
    if T is None: return None, m, None
    xr, yr = C.apply_transform(T, m['xA'], m['yA'])
    res = np.hypot(xr - m['xB'], yr - m['yB'])
    k = res < 0.6
    m2 = {kk: (vv[k] if isinstance(vv, np.ndarray) and vv.shape[:1]==res.shape else vv) for kk, vv in m.items()}
    T2 = C.fit_transform(m2, deg=deg)
    return T2, m, m2

def pair_report(fA, fB, step=24, tag=''):
    t0=time.time()
    T, m0, m2 = refined_transform(fA, fB)
    if T is None: return None
    S = C.sample_pair(fA, fB, T, step=step)
    vA, vB = S['vA'], S['vB']
    L = 0.5 * (vA + vB); d = vA - vB
    # clip extreme pixels (cosmic rays / hot / saturated) robustly
    k = (np.abs(d - np.median(d)) < 8 * 1.4826 * np.median(np.abs(d - np.median(d))) + 1e-9)
    L2, d2 = L[k], d[k]
    g = C.regress_alpha(L2, d2, nb=40)
    # local (patch) alpha
    px = S['xA'][k]; py = S['yA'][k]
    patch = {}
    for (ax, ay) in itertools.product(range(0, 4096, 1024), range(0, 4096, 1024)):
        sel = (px >= ax) & (px < ax + 1024) & (py >= ay) & (py < ay + 1024)
        if sel.sum() < 500: continue
        gg = C.regress_alpha(L2[sel], d2[sel], nb=12, minn=15)
        if gg: patch[(ax, ay)] = gg
    return dict(tag=tag, A=fA['stem'], B=fB['stem'],
                n_stars=int(len(m2['xA'])), n_samp=int(len(L2)),
                rms_ast=float(T['rms']),
                alpha=g['alpha'], beta=g['beta'], r2=g['r2'],
                Lmin=g['Lmin'], Lmax=g['Lmax'], Lmed=g['Lmed'],
                resid_rms=float(np.std(d2 - (g['alpha'] * L2 + g['beta']))),
                medA=float(np.median(vA)), medB=float(np.median(vB)),
                patch={f'{k[0]}_{k[1]}': dict(alpha=v['alpha'], r2=v['r2'], n=v['npts']) for k, v in patch.items()},
                dt=time.time()-t0)

if __name__ == '__main__':
    reg = q3lib.registry()
    for grp in ['t2_m2_red', 't2_m1_red']:
        g = [x for x in reg if x['group'] == grp]
        print('='*100); print(grp, [x['stem'][:40] for x in g])
        for fA, fB in itertools.combinations(g, 2):
            r = pair_report(fA, fB, step=32, tag=grp)
            if r is None: continue
            print('%s vs %s | nstar=%5d nsamp=%6d alpha=%+.5f beta=%+.2f R2=%.4f Lmed=%.1f Lmax=%.1f residRMS=%.2f | medA=%.1f medB=%.1f  [%.1fs]' % (
                r['A'][:28], r['B'][:28], r['n_stars'], r['n_samp'], r['alpha'], r['beta'], r['r2'],
                r['Lmed'], r['Lmax'], r['resid_rms'], r['medA'], r['medB'], r['dt']))
            for kk, vv in sorted(r['patch'].items()):
                print('      patch %-9s alpha=%+.5f R2=%.3f n=%d' % (kk, vv['alpha'], vv['r2'], vv['n']))
