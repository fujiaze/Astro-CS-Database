#!/usr/bin/env python3
"""Q2 stage B: robust tilted boundary loci from nmap."""
import numpy as np, json, os
ROOT = '/workspace/Astro CS Database'
OUT  = ROOT + '/run/RELEASE-02/q2-snr-smooth/realdata'
nmap = np.load(OUT + '/nmap.npy')

def crossing_scan(vals, idx0, thresh, rising):
    """vals: 1-D n array along the scan axis (already ordered along scan dir).
    Return sub-pixel scan coordinate where n crosses thresh (rising or falling), or nan."""
    if rising:
        c = vals >= thresh
    else:
        c = vals <= thresh
    w = np.nonzero(c)[0]
    if w.size == 0 or w[0] == 0:
        return np.nan
    i = w[0]
    n0, n1 = float(vals[i-1]), float(vals[i])
    if n1 == n0:
        return float(idx0 + i)
    frac = (thresh - n0) / (n1 - n0)
    frac = min(max(frac, 0.0), 1.0)
    return float(idx0 + i - 1 + frac)

def robust_fit(xs, ys, n_iter=4, nsig=2.5):
    xs = np.asarray(xs, float); ys = np.asarray(ys, float)
    m = np.isfinite(xs) & np.isfinite(ys)
    xs, ys = xs[m], ys[m]
    if xs.size < 5:
        return None
    for _ in range(n_iter):
        a = np.polyfit(xs, ys, 1)
        r = ys - np.polyval(a, xs)
        s = 1.4826 * np.median(np.abs(r - np.median(r)))
        if s <= 0:
            break
        keep = np.abs(r - np.median(r)) < nsig * s
        if keep.sum() < 5 or keep.all():
            break
        xs, ys = xs[keep], ys[keep]
    a = np.polyfit(xs, ys, 1)
    r = ys - np.polyval(a, xs)
    return dict(slope=float(a[0]), intercept=float(a[1]), n_used=int(xs.size),
                rms=float(np.sqrt(np.mean(r**2))),
                resid_max=float(np.max(np.abs(r))),
                x_min=float(xs.min()), x_max=float(xs.max()))

loci = {}

# ---- H1top: rising n across the upper edge of horizontal band 1, y in [1100,1300] ----
xs = np.arange(700, 2001)
ys = np.array([crossing_scan(nmap[1100:1301, x], 1100, 7.5, True) for x in xs])
fit = robust_fit(xs, ys)
print('H1top raw edge: min %.2f max %.2f  nan %d/%d' % (np.nanmin(ys), np.nanmax(ys), int(np.isnan(ys).sum()), ys.size))
print('  sample:', {int(x): round(float(y), 2) for x, y in zip(xs[::100], ys[::100])})
print('  fit y_edge(x) = %.3f %+.6f x   rms=%.3f maxres=%.2f n=%d  -> y(700)=%.2f y(2000)=%.2f' % (
    fit['intercept'], fit['slope'], fit['rms'], fit['resid_max'], fit['n_used'],
    fit['intercept'] + fit['slope']*700, fit['intercept'] + fit['slope']*2000))
loci['H1top'] = dict(axis='y', scan_range=[1100, 1300], thresh=7.5, rising=True,
                     x_range=[700, 2000], **fit)

# ---- H1bot: falling n across the lower edge of horizontal band 1, y in [1350,1500] ----
ys = np.array([crossing_scan(nmap[1350:1501, x], 1350, 8.5, False) for x in xs])
fitb = robust_fit(xs, ys)
print('H1bot raw edge: min %.2f max %.2f  nan %d/%d' % (np.nanmin(ys), np.nanmax(ys), int(np.isnan(ys).sum()), ys.size))
print('  sample:', {int(x): round(float(y), 2) for x, y in zip(xs[::100], ys[::100])})
print('  fit y_edge(x) = %.3f %+.6f x   rms=%.3f maxres=%.2f n=%d  -> y(700)=%.2f y(2000)=%.2f' % (
    fitb['intercept'], fitb['slope'], fitb['rms'], fitb['resid_max'], fitb['n_used'],
    fitb['intercept'] + fitb['slope']*700, fitb['intercept'] + fitb['slope']*2000))
loci['H1bot'] = dict(axis='y', scan_range=[1350, 1500], thresh=8.5, rising=False,
                     x_range=[700, 2000], **fitb)

# ---- V-right: falling n across the right edge of vertical band, x in [2050,2250] ----
yv = np.arange(300, 3601)
xv = np.array([crossing_scan(nmap[y, 2050:2251], 2050, 8.5, False) for y in yv])
fitv = robust_fit(yv, xv)
print('V-right raw edge: min %.2f max %.2f  nan %d/%d' % (np.nanmin(xv), np.nanmax(xv), int(np.isnan(xv).sum()), xv.size))
print('  sample:', {int(y): round(float(x), 2) for y, x in zip(yv[::300], xv[::300])})
print('  fit x_edge(y) = %.3f %+.6f y   rms=%.3f maxres=%.2f n=%d  -> x(300)=%.2f x(3600)=%.2f' % (
    fitv['intercept'], fitv['slope'], fitv['rms'], fitv['resid_max'], fitv['n_used'],
    fitv['intercept'] + fitv['slope']*300, fitv['intercept'] + fitv['slope']*3600))
loci['V-right'] = dict(axis='x', scan_range=[2050, 2250], thresh=8.5, rising=False,
                       y_range=[300, 3600], **fitv)

with open(OUT + '/loci.json', 'w') as f:
    json.dump(loci, f, indent=2)
print()
print('saved', OUT + '/loci.json')
print(json.dumps(loci, indent=2))
