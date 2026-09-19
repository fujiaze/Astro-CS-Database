#!/usr/bin/env python3
"""Q2 real-data stage A/B: rebuild nmap (stage10 method) + determine tilted boundary loci."""
import numpy as np, glob, warnings, json, os
warnings.filterwarnings('ignore')
from astropy.io import fits
from astropy.wcs import WCS

ROOT = '/workspace/Astro CS Database'
OUT  = ROOT + '/run/RELEASE-02/q2-snr-smooth/realdata'
os.makedirs(OUT, exist_ok=True)

frames = sorted(glob.glob(ROOT + '/run/RELEASE-02/L4-rebuild/norm/*/calibrated_*.fts'))
print('frames found:', len(frames))

w3 = WCS(naxis=2)
w3.wcs.ctype = ['RA---TAN', 'DEC--TAN']
w3.wcs.crpix = [2048.5, 2048.5]
w3.wcs.crval = [83.747318, -5.361391]
w3.wcs.cd = np.array([[-0.00071462, 0.0], [0.0, 0.00071462]])

N = 4096
nmap = np.zeros((N, N), dtype=np.int16)
used = 0; skipped = []
NSAMP = 120          # same as stage10_nmap.py
CHUNK = 512          # row chunk to bound peak memory

for f in frames:
    try:
        wf = WCS(fits.getheader(f))
        t = np.linspace(0, 4095, NSAMP)
        ex = np.concatenate([t, t, t*0+4095, t*0])
        ey = np.concatenate([t*0, t*0+4095, t, t])
        ra, dec = wf.all_pix2world(ex, ey, 0)
        px, py = w3.all_world2pix(ra, dec, 0)
        if not (np.isfinite(px).any() and np.isfinite(py).any()):
            skipped.append((os.path.basename(f), 'all-nan')); continue
        x0 = max(0, int(np.floor(np.nanmin(px)))); x1 = min(N, int(np.ceil(np.nanmax(px)))+1)
        y0 = max(0, int(np.floor(np.nanmin(py)))); y1 = min(N, int(np.ceil(np.nanmax(py)))+1)
        if x1 <= x0 or y1 <= y0:
            skipped.append((os.path.basename(f), 'empty')); continue
        for cy0 in range(y0, y1, CHUNK):
            cy1 = min(y1, cy0 + CHUNK)
            gx, gy = np.meshgrid(np.arange(x0, x1), np.arange(cy0, cy1))
            ra2, dec2 = w3.all_pix2world(gx, gy, 0)
            fx, fy = wf.all_world2pix(ra2, dec2, 0)
            m = (fx >= 0) & (fx <= 4095) & (fy >= 0) & (fy <= 4095) & np.isfinite(fx) & np.isfinite(fy)
            nmap[cy0:cy1, x0:x1] += m.astype(np.int16)
        used += 1
    except Exception as e:
        skipped.append((os.path.basename(f), repr(e)[:80]))

print('used', used, 'skipped', len(skipped))
for s in skipped[:10]:
    print('  SKIP', s)
vals, cnts = np.unique(nmap, return_counts=True)
hist = {int(v): int(c) for v, c in zip(vals, cnts)}
print('nmap hist (n:count):', hist)
print('nmap>0 px:', int((nmap > 0).sum()), ' nmap==0 px:', int((nmap == 0).sum()),
      ' total', N*N)
np.save(OUT + '/nmap.npy', nmap)
print('saved', OUT + '/nmap.npy', nmap.shape, nmap.dtype)

# ---------------- profiles to define the loci ----------------
print()
print('--- n(y) profile: mode/mean over x[900,1900] ---')
prof_y = {}
for y in range(1100, 1461, 10):
    row = nmap[y, 900:1900]
    v, c = np.unique(row, return_counts=True)
    prof_y[y] = (int(v[np.argmax(c)]), float(row.mean()))
    print('  y=%4d mode=%2d mean=%6.2f  n>=8 frac=%.2f' % (y, v[np.argmax(c)], row.mean(), float((row >= 8).mean())))
print()
print('--- n(x) profile: mode/mean over y[300,3600] ---')
for x in range(2040, 2261, 10):
    col = nmap[300:3600, x]
    v, c = np.unique(col, return_counts=True)
    print('  x=%4d mode=%2d mean=%6.2f  n>=8 frac=%.2f' % (x, v[np.argmax(c)], col.mean(), float((col >= 8).mean())))
print()
print('--- n(x) profile over y[500,3300] (narrower, avoids top/bottom band) ---')
for x in range(2040, 2261, 10):
    col = nmap[500:3300, x]
    v, c = np.unique(col, return_counts=True)
    print('  x=%4d mode=%2d mean=%6.2f' % (x, v[np.argmax(c)], col.mean()))
