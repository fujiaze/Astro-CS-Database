#!/usr/bin/env python3
"""Q2 stage C: measure background steps along the tilted seam loci, with off-locus controls."""
import numpy as np, json, os, warnings
warnings.filterwarnings('ignore')
from astropy.io import fits

ROOT = '/workspace/Astro CS Database'
OUT  = ROOT + '/run/RELEASE-02/q2-snr-smooth/realdata'
L4   = ROOT + '/run/RELEASE-02/L4-rebuild'
loci = json.load(open(OUT + '/loci.json'))

PRODUCTS = {
    'p3_r_vis':       L4 + '/p3_r_vis/output_phase3.fits',        # OLD (normalisation reference)
    'p3_r_vis_fixed': L4 + '/p3_r_vis_fixed/output_phase3.fits',  # NEW (bitref_16w)
    'p3_upmfix':      L4 + '/p3_upmfix/output_phase3.fits',       # upmfix_out
}

D = {}
for k, p in PRODUCTS.items():
    with fits.open(p, memmap=True) as h:
        D[k] = np.array(h[0].data, dtype=np.float64)   # 4096^2 f64 = 134 MB each
    print('loaded', k, D[k].shape, 'nan frac %.4f' % float(np.isnan(D[k]).mean()))

K, GAP = 25, 1

def Jy(d, y0, xlo, xhi):
    rL = np.arange(y0-K, y0-GAP); rR = np.arange(y0+GAP+1, y0+K+1)
    L = d[rL[0]:rL[-1]+1, xlo:xhi]; R = d[rR[0]:rR[-1]+1, xlo:xhi]
    ok = np.isfinite(L).all(0) & np.isfinite(R).all(0)
    if ok.sum() < 5: return np.nan
    L = L[:, ok]; R = R[:, ok]
    AL = np.vstack([np.ones_like(rL, float), rL]).T
    p = np.linalg.pinv(AL).T
    eL = L.T @ p; eR = R.T @ p
    return float(np.median((eR[:,0]+eR[:,1]*y0) - (eL[:,0]+eL[:,1]*y0)))

def Jx(d, x0, ylo, yhi):
    cL = np.arange(x0-K, x0-GAP); cR = np.arange(x0+GAP+1, x0+K+1)
    L = d[ylo:yhi, cL[0]:cL[-1]+1]; R = d[ylo:yhi, cR[0]:cR[-1]+1]
    ok = np.isfinite(L).all(1) & np.isfinite(R).all(1)
    if ok.sum() < 5: return np.nan
    L = L[ok]; R = R[ok]
    AL = np.vstack([np.ones_like(cL, float), cL]).T
    p = np.linalg.pinv(AL).T
    eL = L @ p; eR = R @ p
    return float(np.median((eR[:,0]+eR[:,1]*x0) - (eL[:,0]+eL[:,1]*x0)))

def bgwin_y(d, y0, xlo, xhi):
    """OLD median over the union of the two J strips (the local measurement window)."""
    r = np.arange(y0-K, y0+K+1)
    r = r[(r != y0)] if False else r
    w = d[r[0]:r[-1]+1, xlo:xhi]
    return float(np.nanmedian(w))

def bgwin_x(d, x0, ylo, yhi):
    c = np.arange(x0-K, x0+K+1)
    w = d[ylo:yhi, c[0]:c[-1]+1]
    return float(np.nanmedian(w))

def stat(v):
    v = np.asarray([x for x in v if np.isfinite(x)], float)
    if v.size == 0: return dict(n=0, median=None, p16=None, p84=None, mean=None, std=None)
    return dict(n=int(v.size), median=float(np.median(v)),
                p16=float(np.percentile(v, 16)), p84=float(np.percentile(v, 84)),
                mean=float(v.mean()), std=float(v.std(ddof=1)) if v.size > 1 else 0.0)

# ---------------- global OLD background references ----------------
print()
print('--- OLD (p3_r_vis) regional background medians ---')
regions = {
    'H1_out_above y[939,1159] x[900,1900]':  ('y', 939, 1159, 900, 1900),
    'H1_in      y[1219,1356] x[900,1900]':   ('y', 1219, 1356, 900, 1900),
    'H1_out_below y[1416,1636] x[900,1900]': ('y', 1416, 1636, 900, 1900),
    'V_out_left  x[1679,1899] y[300,3600]':  ('x', 1679, 1899, 300, 3600),
    'V_in        x[1959,2103] y[300,3600]':  ('x', 1959, 2103, 300, 3600),
    'V_out_right x[2163,2383] y[300,3600]':  ('x', 2163, 2383, 300, 3600),
}
bg_ref = {}
for name, (ax, a0, a1, b0, b1) in regions.items():
    if ax == 'y': v = float(np.nanmedian(D['p3_r_vis'][a0:a1, b0:b1]))
    else:         v = float(np.nanmedian(D['p3_r_vis'][b0:b1, a0:a1]))
    bg_ref[name] = v
    print('  %-40s %.6g' % (name, v))

# ---------------- measurement ----------------
def measure(locus_name, axis, samples, yb_of, xb_of, shift, prod):
    """axis 'y': horizontal boundary; samples are x. axis 'x': vertical boundary; samples are y."""
    Js, bgs, ns, pos = [], [], [], []
    for s in samples:
        if axis == 'y':
            yb = int(round(yb_of(s) + shift)); best = (0.0, None)
            for yy in range(yb-8, yb+9):
                j = Jy(D[prod], yy, s-50, s+50)
                if np.isfinite(j) and abs(j) > abs(best[0]): best = (j, yy)
            if best[1] is None: continue
            Js.append(best[0]); pos.append(best[1])
            bgs.append(bgwin_y(D['p3_r_vis'], best[1], s-50, s+50))
            ns.append(int(nmapv[best[1], s]))
        else:
            xb = int(round(xb_of(s) + shift)); best = (0.0, None)
            for xx in range(xb-8, xb+9):
                j = Jx(D[prod], xx, s-50, s+50)
                if np.isfinite(j) and abs(j) > abs(best[0]): best = (j, xx)
            if best[1] is None: continue
            Js.append(best[0]); pos.append(best[1])
            bgs.append(bgwin_x(D['p3_r_vis'], best[1], s-50, s+50))
            ns.append(int(nmapv[s, best[1]]))
    return stat(Js), stat(bgs), stat(ns), pos

nmapv = np.load(OUT + '/nmap.npy')

xs = np.arange(750, 1951, 25)
yv = np.arange(400, 3501, 50)
SPEC = {
    'H1top':   dict(axis='y', samples=xs,
                    yb_of=lambda x: loci['H1top']['intercept'] + loci['H1top']['slope']*x),
    'H1bot':   dict(axis='y', samples=xs,
                    yb_of=lambda x: loci['H1bot']['intercept'] + loci['H1bot']['slope']*x),
    'V-right': dict(axis='x', samples=yv,
                    xb_of=lambda y: loci['V-right']['intercept'] + loci['V-right']['slope']*y),
}

BGREF = {'H1top': bg_ref['H1_out_above y[939,1159] x[900,1900]'],
         'H1bot': bg_ref['H1_out_below y[1416,1636] x[900,1900]'],
         'V-right': bg_ref['V_out_right x[2163,2383] y[300,3600]']}

result = {'products': PRODUCTS, 'K': K, 'gap': GAP, 'search_px': 8,
          'bg_ref_old_regional': bg_ref, 'loci': loci, 'measurements': {}}

print()
print('=' * 132)
print('STEP MEASUREMENT  (J = right/lower side minus left/upper side, linear-fit extrapolation to boundary, K=25, gap=1, search +/-8px)')
print('=' * 132)
hdr = '%-14s %-15s %-7s %6s %14s %10s %10s %10s %7s %14s %9s %8s' % (
    'boundary', 'product', 'shift', 'n_s', 'J_median', 'p16', 'p84', 'J/bgLOCAL%', 'n_med', 'bgOLD_local', 'J/bgREF%', 'nmap')
print(hdr)
for bname, spec in SPEC.items():
    axis = spec['axis']
    for shift in (0, -150, 150, -300, 300):
        for prod in ('p3_r_vis_fixed', 'p3_upmfix', 'p3_r_vis'):
            if axis == 'y':
                st, bst, nst, pos = measure(bname, 'y', spec['samples'], spec['yb_of'], None, shift, prod)
            else:
                st, bst, nst, pos = measure(bname, 'x', spec['samples'], None, spec['xb_of'], shift, prod)
            key = '%s|%s|shift%+d' % (bname, prod, shift)
            rec = dict(boundary=bname, axis=axis, product=prod, shift_px=shift,
                       J=st, bg_old_local=bst, nmap=ns2 if False else nst)
            if st['median'] is not None and bst['median']:
                rec['pct_of_local_old_bg'] = 100.0 * st['median'] / bst['median']
                rec['pct_of_ref_old_bg'] = 100.0 * st['median'] / BGREF[bname]
            else:
                rec['pct_of_local_old_bg'] = None
                rec['pct_of_ref_old_bg'] = None
            result['measurements'][key] = rec
            print('%-14s %-15s %+7d %6s %14.5g %10.4g %10.4g %10.4f %7.1f %14.5g %9.4f %8.1f' % (
                bname, prod, shift,
                st['n'],
                st['median'] if st['median'] is not None else float('nan'),
                st['p16'] if st['p16'] is not None else float('nan'),
                st['p84'] if st['p84'] is not None else float('nan'),
                rec['pct_of_local_old_bg'] if rec['pct_of_local_old_bg'] is not None else float('nan'),
                nst['median'] if nst['median'] is not None else float('nan'),
                bst['median'] if bst['median'] is not None else float('nan'),
                rec['pct_of_ref_old_bg'] if rec['pct_of_ref_old_bg'] is not None else float('nan'),
                nst['median'] if nst['median'] is not None else float('nan')))
    print('-' * 132)

with open(OUT + '/stageC_steps.json', 'w') as f:
    json.dump(result, f, indent=2)
print('saved', OUT + '/stageC_steps.json')
