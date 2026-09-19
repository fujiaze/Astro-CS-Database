#!/usr/bin/env python3
"""Q2 synthetic v2: SNR propagation + smoothness at coverage boundaries.

Design so the common-field condition is *representable* by every formulation:
  true sky  S(x,y)  : order-2 polynomial
  frame grad b_k    : order-2 polynomial with DIFFERENT quadratic coeffs per frame
  (a) ref gauge     : global B = order-2 poly  + per-frame delta_k = plane (3)  [production-like]
  (c)/(d)           : per-frame order-2 poly models fit to a running reference
This makes the difference between the formulations one of *gauge/consistency*,
not of raw model capacity.
"""
import numpy as np, json, os

OUT = os.path.dirname(os.path.abspath(__file__))
W, H = 512, 256
xs = np.arange(W); ys = np.arange(H)
X, Y = np.meshgrid(xs, ys)
U = (X - 256.0) / 256.0
V = (Y - 128.0) / 128.0

def poly_cols(u, v, order):
    cols = [np.ones_like(u), u, v]
    if order >= 2:
        cols += [u*u, u*v, v*v]
    return cols

# ---- true sky: order-2 (representable by order-2 models) ----
S = 1000.0 + 3.0*U + 1.5*V + 0.8*U*U + 0.5*U*V - 0.4*V*V

SLABS = [('A', 0, 192), ('B', 128, 320), ('C', 256, 448), ('D', 384, 512)]
NAMES = [s[0] for s in SLABS]
MASK = {n: ((X >= x0) & (X < x1)) for n, x0, x1 in SLABS}
BOUND = [128, 192, 256, 320, 384, 448]

# per-frame gradients: order-2, DIFFERENT quadratic part (this is what a
# plane-delta model cannot absorb)
BCOEF = {
    'A': [0.0,  2.0, -1.0,  1.0,  0.5, -0.7],
    'B': [8.0, -1.5,  1.2, -0.8,  1.4,  0.3],
    'C': [-14.0, 1.0,  0.4,  1.3, -0.9,  0.6],
    'D': [20.0, -0.5, -1.8,  0.6,  0.2,  1.1],
}
def poly_eval(coef, order=2):
    c = list(coef) + [0.0]*(6-len(coef))
    val = c[0] + c[1]*U + c[2]*V
    if order >= 2:
        val = val + c[3]*U*U + c[4]*U*V + c[5]*V*V
    return val
B_TRUE = {n: poly_eval(BCOEF[n]) for n in NAMES}

GAIN    = {'A': 1.0, 'B': 2.0, 'C': 0.8, 'D': 1.0}
SIG_RAW = {'A': 1.0, 'B': 2.6, 'C': 2.0, 'D': 5.0}
SIG_NORM = {n: SIG_RAW[n] / GAIN[n] for n in NAMES}
W_RAW  = {n: 1.0 / SIG_RAW[n]**2 for n in NAMES}
W_NORM = {n: 1.0 / SIG_NORM[n]**2 for n in NAMES}
W_EQ   = {n: 1.0 for n in NAMES}

rng = np.random.default_rng(20260219)
Z = {n: S + B_TRUE[n] + rng.normal(0.0, SIG_NORM[n], (H, W)) for n in NAMES}

# ---------------------------------------------------------------- fitting
def design(m, order):
    return np.stack(poly_cols(U[m], V[m], order), axis=1)

def wls_fit(m, target, w, order):
    A = design(m, order)
    ww = w * np.ones(A.shape[0])
    Hh = (A * ww[:, None]).T @ A
    rh = (A * ww[:, None]).T @ target[m]
    c = np.linalg.solve(Hh, rh)
    cols = poly_cols(U[m], V[m], order)
    return sum(c[i]*cols[i] for i in range(len(cols)))

def joint_ref_fit(weights, ref='A', B_order=2, d_order=1):
    """Fit z_k = B(x) + delta_k(x), delta_ref=0.  B: global poly (B_order),
    delta_k: per-frame poly (d_order).  Weighted LS."""
    nB = len(poly_cols(np.array([0.0]), np.array([0.0]), B_order))
    nd = len(poly_cols(np.array([0.0]), np.array([0.0]), d_order))
    nonref = [n for n in NAMES if n != ref]
    ncol = nB + nd*len(nonref)
    Hm = np.zeros((ncol, ncol)); rh = np.zeros(ncol)
    for n in NAMES:
        m = MASK[n]
        Ab = np.stack(poly_cols(U[m], V[m], B_order), axis=1)
        A = np.zeros((Ab.shape[0], ncol))
        A[:, :nB] = Ab
        if n != ref:
            j = nonref.index(n); c0 = nB + nd*j
            dcols = poly_cols(U[m], V[m], d_order)
            for q in range(nd):
                A[:, c0 + q] = dcols[q]
        w = weights[n] * np.ones(A.shape[0])
        Hm += (A * w[:, None]).T @ A
        rh += (A * w[:, None]).T @ Z[n][m]
    theta = np.linalg.solve(Hm, rh)
    Bf = sum(theta[i]*poly_cols(U, V, B_order)[i] for i in range(nB))
    delta = {}
    for n in NAMES:
        if n == ref:
            delta[n] = np.zeros((H, W))
        else:
            j = nonref.index(n); c0 = nB + nd*j
            delta[n] = sum(theta[c0+q]*poly_cols(U, V, d_order)[q] for q in range(nd))
    return delta, Bf

def gauss_seidel(wref, order=2, exclude_self=True, niter=200, tol=1e-13):
    c = {n: np.zeros((H, W)) for n in NAMES}
    hist = []
    for it in range(niter):
        res = {n: Z[n] - c[n] for n in NAMES}
        newc = {}
        for n in NAMES:
            m = MASK[n]
            num = np.zeros((H, W)); den = np.zeros((H, W))
            for j in NAMES:
                if exclude_self and j == n:
                    continue
                mj = MASK[j]
                num[mj] += wref[j] * res[j][mj]
                den[mj] += wref[j]
            tm = m & (den > 0)
            if tm.sum() < 6:
                newc[n] = c[n]; continue
            target = Z[n] - np.where(den > 0, num / np.maximum(den, 1e-300), 0.0)
            ck = c[n].copy()
            ck[tm] = wls_fit(tm, target, wref[n], order)
            newc[n] = ck
        chg = max(float(np.max(np.abs(newc[n]-c[n]))) for n in NAMES)
        c = newc; hist.append(chg)
        if chg < tol: break
    return c, hist

def resid_field(c, wref):
    num = np.zeros((H, W)); den = np.zeros((H, W))
    for n in NAMES:
        m = MASK[n]
        num[m] += wref[n] * (B_TRUE[n][m] - c[n][m])
        den[m] += wref[n]
    return num / np.maximum(den, 1e-300)

def stack_field(c, wstack):
    num = np.zeros((H, W)); den = np.zeros((H, W))
    for n in NAMES:
        m = MASK[n]
        num[m] += wstack[n] * (Z[n][m] - c[n][m])
        den[m] += wstack[n]
    return np.where(den > 0, num / np.maximum(den, 1e-300), np.nan)

def step(field, xb, half=16, gap=4):
    lo = field[:, xb-half:xb-gap]; hi = field[:, xb+gap:xb+half]
    return float(np.nanmedian(hi) - np.nanmedian(lo))

def steps_of(field):
    return {str(xb): step(field, xb) for xb in BOUND}

def summarize(c, wref, wstack):
    R = resid_field(c, wref)
    st = stack_field(c, wstack)
    spread = np.std([B_TRUE[n]-c[n] for n in NAMES], axis=0)
    return {'resid_step_ADU': steps_of(R),
            'resid_step_absmax_ADU': float(max(abs(v) for v in steps_of(R).values())),
            'resid_rms_ADU': float(np.sqrt(np.nanmean((R - np.nanmedian(R))**2))),
            'common_spread_med_ADU': float(np.nanmedian(spread)),
            'common_spread_p90_ADU': float(np.nanpercentile(spread, 90)),
            'stack_step_ADU': steps_of(st),
            'stack_step_absmax_ADU': float(max(abs(v) for v in steps_of(st).values())),
            'stack_minus_S_rms_ADU': float(np.sqrt(np.nanmean((st - S)**2)))}

def run(tag, wfit, order=2, B_order=2, d_order=1, wstack=None, final_gauge=False, wmap=None):
    if wstack is None: wstack = wfit
    d, Bf = joint_ref_fit(wfit, B_order=B_order, d_order=d_order)
    ca = d
    # (b) global weighted-mean-zero on residuals g_k = b_k - delta_k
    num = sum(float(np.sum(wfit[n]*(B_TRUE[n]-d[n])[MASK[n]])) for n in NAMES)
    den = sum(float(np.sum(wfit[n]*MASK[n])) for n in NAMES)
    gscal = num/den
    cb = {n: d[n] + gscal for n in NAMES}
    cc, hc = gauss_seidel(wfit, order=order, exclude_self=True)
    cd, hd = gauss_seidel(wfit, order=order, exclude_self=False)
    out = {'tag': tag, 'order': order, 'B_order': B_order, 'd_order': d_order,
           'global_gauge_shift_ADU': gscal,
           'iters_excl': len(hc), 'iters_incl': len(hd)}
    # optional final common-field gauge applied to BOTH c and d
    if final_gauge:
        Rc = resid_field(cc, wfit); cc = {n: cc[n] + Rc for n in NAMES}
        Rd = resid_field(cd, wfit); cd = {n: cd[n] + Rd for n in NAMES}
    # stack weight map (spatially varying)
    if wmap is None:
        ws = wstack
    else:
        ws = {n: wstack[n]*wmap[n] for n in NAMES}
    out['a_ref'] = summarize(ca, wfit, ws)
    out['b_global_mean0'] = summarize(cb, wfit, ws)
    out['c_excl_self'] = summarize(cc, wfit, ws)
    out['d_incl_self'] = summarize(cd, wfit, ws)
    return out

results = {}

# ---------------- item 2: weight ordering ----------------
order_raw = sorted(NAMES, key=lambda n: -W_RAW[n])
order_norm = sorted(NAMES, key=lambda n: -W_NORM[n])
def kendall(a, b):
    conc = disc = 0
    for i in range(len(a)):
        for j in range(i+1, len(a)):
            s = np.sign(a[i]-a[j])*np.sign(b[i]-b[j])
            if s > 0: conc += 1
            elif s < 0: disc += 1
    return (conc-disc)/(conc+disc)
ra = [W_RAW[n] for n in NAMES]; rn = [W_NORM[n] for n in NAMES]
results['weight_order'] = {
    'frames': NAMES, 'gain': GAIN, 'sigma_raw': SIG_RAW,
    'sigma_norm': {n: SIG_NORM[n] for n in NAMES},
    'w_raw_1_over_sigma_raw2': W_RAW, 'w_norm_1_over_sigma_norm2': W_NORM,
    'rank_by_w_raw': order_raw, 'rank_by_w_norm': order_norm,
    'kendall_tau_raw_vs_norm': float(kendall(ra, rn)),
    'order_flipped': bool(order_raw != order_norm),
    'A_over_B_w_raw': float(W_RAW['A']/W_RAW['B']),
    'A_over_B_w_norm': float(W_NORM['A']/W_NORM['B'])}
print('=== item2 weight ordering ==='); print(json.dumps(results['weight_order'], indent=1))

def show(tag, sc):
    print('== %s  gauge_shift=%+.3f iters %d/%d' % (tag, sc['global_gauge_shift_ADU'], sc['iters_excl'], sc['iters_incl']))
    for f in ['a_ref','b_global_mean0','c_excl_self','d_incl_self']:
        x = sc[f]
        print('   %-16s spread=%7.4f residRMS=%7.4f residMaxStep=%7.3f stackMaxStep=%7.3f' % (
            f, x['common_spread_med_ADU'], x['resid_rms_ADU'], x['resid_step_absmax_ADU'], x['stack_step_absmax_ADU']))
        print('        resid_steps', ' '.join('%+.3f'%x['resid_step_ADU'][str(b)] for b in BOUND))

print('\n=== SC1 ord2 models, fit=stack weights=SNR-normalized ===')
sc1 = run('SC1', W_NORM, order=2, wstack=W_NORM); show('SC1', sc1); results['SC1'] = sc1

print('\n=== SC2 ord2 models, equal weights ===')
sc2 = run('SC2', W_EQ, order=2, wstack=W_EQ); show('SC2', sc2); results['SC2'] = sc2

print('\n=== SC3 production-like: B=ord2 global, delta=plane, SNR ===')
sc3 = run('SC3', W_NORM, order=2, B_order=2, d_order=1, wstack=W_NORM); show('SC3', sc3); results['SC3'] = sc3

print('\n=== SC4 B=plane(ord1) global, delta=plane -> severe capacity limit ===')
sc4 = run('SC4', W_NORM, order=2, B_order=1, d_order=1, wstack=W_NORM); show('SC4', sc4); results['SC4'] = sc4

print('\n=== SC5 SC1 + final common-field gauge ===')
sc5 = run('SC5', W_NORM, order=2, wstack=W_NORM, final_gauge=True); show('SC5', sc5); results['SC5'] = sc5

# ---------------- item 5: spatially varying stack weight map ----------------
# frame B weight x3 for x>256 (a boundary at 256)
wmap = {n: np.ones((H, W)) for n in NAMES}
wmap['B'] = np.where(X > 256, 3.0, 1.0)
print('\n=== SC6 SC1 with spatially varying stack weight (B x3 for x>256) ===')
sc6 = run('SC6', W_NORM, order=2, wstack=W_NORM, wmap=wmap); show('SC6', sc6); results['SC6'] = sc6
print('  step change vs SC1 (stack, absmax per formulation):')
for f in ['a_ref','c_excl_self','d_incl_self']:
    print('   %-16s %+.4f' % (f, sc6[f]['stack_step_absmax_ADU'] - sc1[f]['stack_step_absmax_ADU']))

with open(os.path.join(OUT,'synth_results_v2.json'),'w') as fo:
    json.dump(results, fo, indent=1)
print('\nwrote synth_results_v2.json')
