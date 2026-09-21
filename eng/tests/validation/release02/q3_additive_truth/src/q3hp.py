"""High-pass (fixed-effects) estimator of alpha in  d = alpha*L + beta(p).
beta(p) is allowed to be an arbitrary additive field that is constant within a
window; removing the per-window median of d and L removes it.
"""
import sys
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np

def hp_alpha(xA, yA, d, L, win=256, clip=5.0, iters=2):
    """Fixed-effects alpha. Returns dict(alpha, n, rms, nbins)."""
    xA = np.asarray(xA); yA = np.asarray(yA); d = np.asarray(d); L = np.asarray(L)
    wx = np.floor(xA / win).astype(np.int64)
    wy = np.floor(yA / win).astype(np.int64)
    key = wx * 100000 + wy
    uk, inv = np.unique(key, return_inverse=True)
    m = np.isfinite(d) & np.isfinite(L)
    d = d[m]; L = L[m]; inv = inv[m]
    # per-window median
    nw = len(uk)
    dm = np.zeros(nw); Lm = np.zeros(nw)
    order = np.argsort(inv, kind='stable')
    bounds = np.searchsorted(inv[order], np.arange(nw + 1))
    for i in range(nw):
        idx = order[bounds[i]:bounds[i + 1]]
        if len(idx) < 8:
            dm[i] = np.nan; Lm[i] = np.nan; continue
        dm[i] = np.median(d[idx]); Lm[i] = np.median(L[idx])
    dd = d - dm[inv]; LL = L - Lm[inv]
    keep = np.isfinite(dd) & np.isfinite(LL)
    dd = dd[keep]; LL = LL[keep]
    alpha = float(np.sum(dd * LL) / np.sum(LL * LL))
    for _ in range(iters):
        res = dd - alpha * LL
        s = 1.4826 * np.median(np.abs(res - np.median(res)))
        if s <= 0: break
        kk = np.abs(res) < clip * s
        alpha = float(np.sum(dd[kk] * LL[kk]) / np.sum(LL[kk] ** 2))
    res = dd - alpha * LL
    return dict(alpha=alpha, n=int(len(dd)), nbins=int(nw),
                rms=float(np.std(res)), Lrms=float(np.std(LL)), drms=float(np.std(dd)))
