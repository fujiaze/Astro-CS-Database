"""Synthetic control: does the alpha criterion have discriminating power?
World (i): inter-frame difference is PURE ADDITIVE (P true).
World (ii): difference is modulated by a multiplicative response (P false).
Also tests the estimator variants (global bin-median on L / on vB, fixed-effects).
"""
import sys, os
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C, q3hp

OUT = 'run/RELEASE-02/q3-additive-truth/data'
os.makedirs(OUT, exist_ok=True)
RNG = np.random.default_rng(20250919)

def make_scene(size=1024, seed=1):
    """Analytic scene: smooth nebula + point sources + noise."""
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    u = (xx - size / 2) / (size / 2); v = (yy - size / 2) / (size / 2)
    r = np.hypot(u, v)
    neb = 1200.0 * np.exp(-(r / 0.55) ** 2) + 350.0 * np.exp(-((u + 0.4) ** 2 + (v - 0.3) ** 2) / 0.08)
    # point sources
    ps = np.zeros_like(neb)
    for _ in range(400):
        cx = rng.uniform(0, size); cy = rng.uniform(0, size); f = rng.uniform(1e3, 5e5)
        s = 1.6
        ps += f / (2 * np.pi * s * s) * np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (2 * s * s))
    return neb + ps

def add_noise(img, sky, sigma=20.0, rng=None):
    rng = rng or RNG
    return img + sky + rng.normal(0, sigma, img.shape)

def estimate(fname, yA, yB, gx, gy, alpha_true):
    """Run the criteria on gridded samples."""
    vA = yA[gy][:, gx].ravel(); vB = yB[gy][:, gx].ravel()
    X, Y = np.meshgrid(gx.astype(float), gy.astype(float))
    xA = X.ravel(); yA_ = Y.ravel()
    L = 0.5 * (vA + vB); d = vA - vB
    res = {}
    gL = C.regress_alpha(L, d, nb=30, minn=10)
    res['global_on_L'] = gL['alpha'] if gL else np.nan
    gB = C.regress_alpha(vB, d, nb=30, minn=10)
    res['global_on_vB'] = gB['alpha'] if gB else np.nan
    for w in [64, 128, 256]:
        res['hp_L_w%d' % w] = q3hp.hp_alpha(xA, yA_, d, L, win=w)['alpha']
        res['hp_vB_w%d' % w] = q3hp.hp_alpha(xA, yA_, d, vB, win=w)['alpha']
    # scalar normalisation counterfactual: divide A by star-equivalent scalar
    # (here the "scalar" is the median ratio measured on bright samples)
    bright = L > np.percentile(L, 80)
    a_est = float(np.median(vA[bright] / vB[bright]))
    vA1 = vA / a_est
    res['scalar_a_est'] = a_est
    res['hp_L_after_scalar'] = q3hp.hp_alpha(xA, yA_, vA1 - vB, 0.5 * (vA1 + vB), win=128)['alpha']
    res['alpha_true'] = alpha_true
    return res

if __name__ == '__main__':
    size = 1024
    T = make_scene(size)
    g = np.arange(8, size - 8, 4)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    u = (xx - size / 2) / (size / 2); v = (yy - size / 2) / (size / 2)
    r = np.hypot(u, v)
    # additive-only world: identical multiplicative response (a=1), different skies
    SA = 200.0 + 8.0 * u + 4.0 * v
    SB = 160.0 - 3.0 * u + 6.0 * v
    yA = add_noise(T, SA, 20.0, np.random.default_rng(11))
    yB = add_noise(T, SB, 20.0, np.random.default_rng(12))
    r1 = estimate('world_i', yA, yB, g, g, 0.0)
    print('WORLD (i) pure additive (alpha_true=0):')
    for k in ['global_on_L', 'global_on_vB', 'hp_L_w64', 'hp_L_w128', 'hp_L_w256', 'hp_vB_w64', 'hp_vB_w128', 'hp_vB_w256', 'scalar_a_est', 'hp_L_after_scalar']:
        print('   %-20s %+.5f' % (k, r1[k]))
    # multiplicative world: a_A varies (constant 1.15 + vignetting), a_B = 1
    aA = 1.15 * (1.0 + 0.10 * r ** 2)
    aB = 1.00 * (1.0 - 0.02 * r ** 2)
    yA2 = add_noise(aA * T, SA, 20.0, np.random.default_rng(21))
    yB2 = add_noise(aB * T, SB, 20.0, np.random.default_rng(22))
    r2 = estimate('world_ii', yA2, yB2, g, g, 0.15)
    print('WORLD (ii) multiplicative (median a-1 ~ +0.15, spatial +-10%):')
    for k in ['global_on_L', 'global_on_vB', 'hp_L_w64', 'hp_L_w128', 'hp_L_w256', 'hp_vB_w64', 'hp_vB_w128', 'hp_vB_w256', 'scalar_a_est', 'hp_L_after_scalar']:
        print('   %-20s %+.5f' % (k, r2[k]))
    print()
    print('MEDIAN a_true (signal-weighted) = %.4f' % float(np.median(aA / aB)))
