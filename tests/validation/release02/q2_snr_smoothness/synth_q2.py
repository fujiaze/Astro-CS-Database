#!/usr/bin/env python3
"""Q2 synthetic experiment: SNR propagation into the additive-gradient fit
and smoothness (no step) at frame-coverage boundaries.

Known truth: common sky S(x,y), per-frame gradients b_k(x,y), raw sigma, gain.
Normalized frame  z_k = raw_k / g_k = S + b_k + N(0, (sigma_raw/g)^2).
Correction model c_k (per frame).  Residual gradient g_k = b_k - c_k.
Mosaic residual  R(p) = sum_{k in S(p)} w_k g_k / W_S(p).
A coverage boundary produces a visible seam iff R jumps there.
"""
import numpy as np, json, os, sys

OUT = os.path.dirname(os.path.abspath(__file__))
W, H = 512, 256
xs = np.arange(W); ys = np.arange(H)
X, Y = np.meshgrid(xs, ys)
U = (X - 256.0) / 256.0
V = (Y - 128.0) / 128.0
S = 1000.0 + 0.004 * X + 2.0 * np.sin(2 * np.pi * X / 512.0) + 0.8 * np.cos(2 * np.pi * Y / 256.0)

SLABS = [('A', 0, 192), ('B', 128, 320), ('C', 256, 448), ('D', 384, 512)]
NAMES = [s[0] for s in SLABS]
MASK = {n: ((X >= x0) & (X < x1)) for n, x0, x1 in SLABS}
BOUND = [128, 192, 256, 320, 384, 448]

COEF = {
    'A': [0.0, 2.0, -1.0, 1.0, 0.5, -0.7],
    'B': [8.0, -1.5, 1.2, -0.8, 1.4, 0.3],
    'C': [-14.0, 1.0, 0.4, 1.3, -0.9, 0.6],
    'D': [20.0, -0.5, -1.8, 0.6, 0.2, 1.1],
}
def grad(n):
    c = COEF[n]
    return c[0] + c[1]*U + c[2]*V + c[3]*U*U + c[4]*U*V + c[5]*V*V
B_TRUE = {n: grad(n) for n in NAMES}

GAIN    = {'A': 1.0, 'B': 2.0, 'C': 0.8, 'D': 1.0}
SIG_RAW = {'A': 1.0, 'B': 2.6, 'C': 2.0, 'D': 5.0}
SIG_NORM = {n: SIG_RAW[n] / GAIN[n] for n in NAMES}
W_RAW  = {n: 1.0 / SIG_RAW[n]**2 for n in NAMES}
W_NORM = {n: 1.0 / SIG_NORM[n]**2 for n in NAMES}
W_EQ   = {n: 1.0 for n in NAMES}

rng = np.random.default_rng(20260219)
Z = {n: S + B_TRUE[n] + rng.normal(0.0, SIG_NORM[n], (H, W)) for n in NAMES}

# ---------------------------------------------------------------- helpers
def bilinear_design(Xs, Ys, nx, ny):
    xn = np.linspace(0.0, W, nx); yn = np.linspace(0.0, H, ny)
    ix = np.clip(np.searchsorted(xn, Xs) - 1, 0, nx - 2)
    iy = np.clip(np.searchsorted(yn, Ys) - 1, 0, ny - 2)
    fx = (Xs - xn[ix]) / (xn[ix + 1] - xn[ix])
    fy = (Ys - yn[iy]) / (yn[iy + 1] - yn[iy])
    idx = np.stack([iy*nx+ix, iy*nx+ix+1, (iy+1)*nx+ix, (iy+1)*nx+ix+1], axis=1)
    wt  = np.stack([(1-fx)*(1-fy), fx*(1-fy), (1-fx)*fy, fx*fy], axis=1)
    return idx, wt

def eval_grid(theta, nx, ny):
    idx, wt = bilinear_design(X.ravel().astype(float), Y.ravel().astype(float), nx, ny)
    return np.sum(theta[idx] * wt, axis=1).reshape(H, W)

def joint_ref_fit(weights, nx=9, ny=5, ref='A'):
    """Fit z_k = B(x) + delta_k(x), delta_ref=0, weighted LS. Returns (delta, B)."""
    nB = nx * ny
    nonref = [n for n in NAMES if n != ref]
    ncol = nB + 3 * len(nonref)
    Hm = np.zeros((ncol, ncol)); rh = np.zeros(ncol)
    for n in NAMES:
        m = MASK[n]
        Xs = X[m].astype(float); Ys = Y[m].astype(float)
        idx, wt = bilinear_design(Xs, Ys, nx, ny)
        w = weights[n] * np.ones(Xs.size)
        if n == ref:
            cols = idx; vals = wt
        else:
            j = nonref.index(n); c0 = nB + 3*j
            dcols = np.stack([np.full(Xs.size, c0), np.full(Xs.size, c0+1), np.full(Xs.size, c0+2)], axis=1)
            dvals = np.stack([np.ones(Xs.size), U[m], V[m]], axis=1)
            cols = np.concatenate([idx, dcols], axis=1)
            vals = np.concatenate([wt, dvals], axis=1)
        zn = Z[n][m]
        nz = cols.shape[1]
        for a in range(nz):
            ca = cols[:, a]; va = vals[:, a]
            rh += np.bincount(ca, weights=w*va*zn, minlength=ncol)
            for b in range(nz):
                cb = cols[:, b]; vb = vals[:, b]
                fl = ca.astype(np.int64) * ncol + cb
                Hm += np.bincount(fl, weights=w*va*vb, minlength=ncol*ncol).reshape(ncol, ncol)
    theta = np.linalg.solve(Hm, rh)
    Bf = eval_grid(theta[:nB], nx, ny)
    delta = {}
    for n in NAMES:
        if n == ref:
            delta[n] = np.zeros((H, W))
        else:
            j = nonref.index(n); c0 = nB + 3*j
            delta[n] = theta[c0] + theta[c0+1]*U + theta[c0+2]*V
    return delta, Bf

def design(m, order):
    u = U[m]; v = V[m]
    cols = [np.ones_like(u), u, v]
    if order >= 2:
        cols += [u*u, u*v, v*v]
    return np.stack(cols, axis=1)

def wls_fit(m, target, w, order):
    A = design(m, order)
    ww = w * np.ones(A.shape[0])
    Hh = (A * ww[:, None]).T @ A
    rh = (A * ww[:, None]).T @ target[m]
    c = np.linalg.solve(Hh, rh)
    u = U[m]; v = V[m]
    cols = [np.ones_like(u), u, v]
    if order >= 2:
        cols += [u*u, u*v, v*v]
    return sum(c[i]*cols[i] for i in range(len(cols))).reshape(-1)

def gauss_seidel(wref, order=1, exclude_self=True, niter=80, tol=1e-12):
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
        c = newc
        hist.append(chg)
        if chg < tol:
            break
    return c, hist

def resid_field(c, wref):
    num = np.zeros((H, W)); den = np.zeros((H, W))
    for n in NAMES:
        m = MASK[n]
        num[m] += wref[n] * (B_TRUE[n][m] - c[n][m])
        den[m] += wref[n]
    return num / np.maximum(den, 1e-300), den

def stack_field(c, wstack):
    num = np.zeros((H, W)); den = np.zeros((H, W))
    for n in NAMES:
        m = MASK[n]
        num[m] += wstack[n] * (Z[n][m] - c[n][m])
        den[m] += wstack[n]
    return np.where(den > 0, num / np.maximum(den, 1e-300), np.nan), den

def step(field, xb, half=16, gap=4):
    lo = field[:, xb-half:xb-gap]; hi = field[:, xb+gap:xb+half]
    return float(np.nanmedian(hi) - np.nanmedian(lo))

def steps_of(field):
    return {str(xb): step(field, xb) for xb in BOUND}

def summarize(name, c, wref, wstack=None):
    R, _ = resid_field(c, wref)
    out = {'formulation': name,
           'resid_step_ADU': steps_of(R),
           'resid_rms_ADU': float(np.sqrt(np.nanmean((R - np.nanmedian(R))**2))),
           'common_spread_ADU': float(np.nanmedian(np.std([B_TRUE[n]-c[n] for n in NAMES], axis=0)))}
    if wstack is not None:
        st, _ = stack_field(c, wstack)
        out['stack_step_ADU'] = steps_of(st)
        out['stack_minus_S_rms_ADU'] = float(np.sqrt(np.nanmean((st - S)**2)))
    return out

results = {}

# ---- weight ordering (item 2) ----
order_raw = sorted(NAMES, key=lambda n: -W_RAW[n])
order_norm = sorted(NAMES, key=lambda n: -W_NORM[n])
def kendall(a, b):
    n = len(a); conc = disc = 0
    for i in range(n):
        for j in range(i+1, n):
            s = np.sign(a[i]-a[j]) * np.sign(b[i]-b[j])
            if s > 0: conc += 1
            elif s < 0: disc += 1
    return (conc - disc) / (conc + disc)
ra = [W_RAW[n] for n in NAMES]; rn = [W_NORM[n] for n in NAMES]
results['weight_order'] = {
    'frames': NAMES,
    'gain': GAIN, 'sigma_raw': SIG_RAW, 'sigma_norm': {n: SIG_NORM[n] for n in NAMES},
    'w_raw_1_over_sigma_raw2': W_RAW,
    'w_norm_1_over_sigma_norm2': W_NORM,
    'rank_by_w_raw': order_raw,
    'rank_by_w_norm': order_norm,
    'kendall_tau_raw_vs_norm': float(kendall(ra, rn)),
    'order_flipped': bool(order_raw != order_norm),
    'A_over_B_w_raw': float(W_RAW['A']/W_RAW['B']),
    'A_over_B_w_norm': float(W_NORM['A']/W_NORM['B']),
}
print('=== item2 weight ordering ===')
print(json.dumps(results['weight_order'], indent=1))

# ---- scenarios ----
def run_scenario(tag, wfit, order, nx=9, ny=5, wstack=None, apply_common_gauge=False):
    d, Bf = joint_ref_fit(wfit, nx=nx, ny=ny)
    # (a) ref gauge, (b) global weighted-mean-zero gauge
    tot = sum(float(np.sum(wfit[n] * d[n][MASK[n]])) for n in NAMES)
    totw = sum(float(np.sum(wfit[n] * MASK[n])) for n in NAMES)
    gmean = tot / totw
    d_b = {n: d[n] - gmean for n in NAMES}
    ca = d; cb = d_b
    cc, hist = gauss_seidel(wfit, order=order, exclude_self=True)
    cd, hist2 = gauss_seidel(wfit, order=order, exclude_self=False)
    if apply_common_gauge:
        Rc, _ = resid_field(cc, wfit)
        cc = {n: cc[n] + Rc for n in NAMES}
    res = {'tag': tag, 'order': order, 'wfit': 'given',
           'gauge_shift_b': gmean,
           'gs_iter_excl': len(hist), 'gs_iter_incl': len(hist2),
           'a_ref': summarize('a_ref', ca, wfit, wstack),
           'b_global_mean0': summarize('b_global_mean0', cb, wfit, wstack),
           'c_excl_self': summarize('c_excl_self', cc, wfit, wstack),
           'd_incl_self': summarize('d_incl_self', cd, wfit, wstack)}
    return res

print('\n=== SC1: model=plane(ord1), fit weights = SNR (correct normalized) ===')
sc1 = run_scenario('SC1_ord1_snr', W_NORM, 1, wstack=W_NORM)
print(json.dumps(sc1, indent=1))
results['SC1'] = sc1

print('\n=== SC2: model=plane(ord1), fit weights = equal ===')
sc2 = run_scenario('SC2_ord1_eq', W_EQ, 1, wstack=W_EQ)
print(json.dumps(sc2, indent=1))
results['SC2'] = sc2

print('\n=== SC3: model=order2 (capacity sufficient), fit weights = SNR ===')
sc3 = run_scenario('SC3_ord2_snr', W_NORM, 2, wstack=W_NORM)
print(json.dumps(sc3, indent=1))
results['SC3'] = sc3

print('\n=== SC4: model=plane, flexible B (nx=17,ny=9), SNR ===')
sc4 = run_scenario('SC4_ord1_snr_flexB', W_NORM, 1, nx=17, ny=9, wstack=W_NORM)
print(json.dumps(sc4, indent=1))
results['SC4'] = sc4

print('\n=== SC5: SC1 + final common gauge on (c) ===')
sc5 = run_scenario('SC5_ord1_snr_finalgauge', W_NORM, 1, wstack=W_NORM, apply_common_gauge=True)
print(json.dumps(sc5, indent=1))
results['SC5'] = sc5

with open(os.path.join(OUT, 'synth_results.json'), 'w') as f:
    json.dump(results, f, indent=1)
print('\nwrote', os.path.join(OUT, 'synth_results.json'))
