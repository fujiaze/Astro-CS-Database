"""Q3 core: pairwise frame analysis on the native calibrated frames.

All coordinates are 0-based array indices.  Relative astrometry between two
frames is taken from an empirical polynomial fit (matched bright stars) so the
sampling of "the same sky position" is good to <<0.1 px even though the solved
WCS pair differs by ~1 px RMS.
"""
import json, os
import numpy as np
from astropy.io import fits
from astropy.wcs import WCS
from astropy.coordinates import SkyCoord
import astropy.units as u
import q3lib

# ---------- WCS ----------
def wcs_lin(frame):
    d = json.load(open(frame['wcs_json'])); w = d['wcs']
    return WCS(dict(CTYPE1='RA---TAN', CTYPE2='DEC--TAN',
                    CRPIX1=w['crpix1'], CRPIX2=w['crpix2'],
                    CRVAL1=w['crval1'], CRVAL2=w['crval2'],
                    CD1_1=w['cd11'], CD1_2=w['cd12'],
                    CD2_1=w['cd21'], CD2_2=w['cd22'],
                    NAXIS=2, NAXIS1=4096, NAXIS2=4096))

def wcs_sip(frame):
    """Effective drizzle WCS: TAN + SIP A/B (FITS convention, dx = xp-crpix)."""
    d = json.load(open(frame['wcs_json'])); w = d['wcs']; s = w.get('sip')
    W = wcs_lin(frame)
    if s:
        W.sip = __import__('astropy.wcs', fromlist=['Sip']).Sip(
            np.array(s['a']).reshape(6, 6), np.array(s['b']).reshape(6, 6),
            np.array(s['ap']).reshape(6, 6), np.array(s['bp']).reshape(6, 6), W.wcs.crpix)
    return W

# ---------- frame data ----------
_FRAME_CACHE = {}
def frame_data(frame):
    k = frame['path']
    if k not in _FRAME_CACHE:
        _FRAME_CACHE[k] = fits.getdata(frame['path'], memmap=True)
    return _FRAME_CACHE[k]

_FLUX_CACHE = {}
def flux_all(frame):
    """Load the whole group flux json once; return per-frame catalog dict."""
    pj = os.path.join(os.path.dirname(frame['path']), 'p1_flux.json')
    if pj not in _FLUX_CACHE:
        d = json.load(open(pj))
        cat = {}
        for fr in d['frames']:
            stem = fr['file'].replace('cleaned_', '').replace('.fts', '')
            res = fr['results']
            cat[stem] = dict(
                x=np.array([r['x'] for r in res], float),
                y=np.array([r['y'] for r in res], float),
                flux=np.array([r['flux'] for r in res], float),
                snr=np.array([r['snr'] for r in res], float),
                valid=np.array([r['valid'] for r in res], bool),
                bg=np.array([r['background'] for r in res], float))
        _FLUX_CACHE[pj] = cat
    return _FLUX_CACHE[pj][frame['stem']]

def free_frames():
    _FRAME_CACHE.clear()

# ---------- star matching / relative astrometry ----------
def match_stars(fA, fB, snr_min=50.0, rad_arcsec=2.0, wcsf=None):
    if wcsf is None:
        wcsf = wcs_sip   # effective drizzle WCS: TAN+SIP is REQUIRED for cross-panel matching
    """Pre-filter by SNR/valid before the O(n*m) sky match (speed)."""
    ca, cb = flux_all(fA), flux_all(fB)
    ka = np.where(ca['valid'] & (ca['snr'] > snr_min))[0]
    kb = np.where(cb['valid'] & (cb['snr'] > snr_min))[0]
    WA, WB = wcsf(fA), wcsf(fB)
    raA, decA = WA.all_pix2world(ca['x'][ka], ca['y'][ka], 0)
    raB, decB = WB.all_pix2world(cb['x'][kb], cb['y'][kb], 0)
    sA = SkyCoord(raA * u.deg, decA * u.deg); sB = SkyCoord(raB * u.deg, decB * u.deg)
    idx, d2d, _ = sA.match_to_catalog_sky(sB)
    m = d2d.arcsec < rad_arcsec
    iA = ka[m]; iB = kb[idx[m]]
    return dict(iA=iA, iB=iB, sep_arcsec=d2d.arcsec[m],
                xA=ca['x'][iA], yA=ca['y'][iA],
                xB=cb['x'][iB], yB=cb['y'][iB],
                fA=ca['flux'][iA], fB=cb['flux'][iB],
                snrA=ca['snr'][iA], snrB=cb['snr'][iB])

def fit_transform(match, deg=1, nmin=30):
    """Polynomial map A-pixel -> B-pixel (normalized coords, centred 2048)."""
    xa, ya = match['xA'], match['yA']
    u_ = (xa - 2048.0) / 2048.0; v_ = (ya - 2048.0) / 2048.0
    terms = [np.ones_like(u_)]
    for p in range(deg + 1):
        for q in range(deg + 1 - p):
            if p + q == 0: continue
            terms.append(u_ ** p * v_ ** q)
    M = np.vstack(terms).T
    if M.shape[0] < nmin:
        return None
    cx, *_ = np.linalg.lstsq(M, match['xB'], rcond=None)
    cy, *_ = np.linalg.lstsq(M, match['yB'], rcond=None)
    rx = M @ cx - match['xB']; ry = M @ cy - match['yB']
    return dict(cx=cx, cy=cy, deg=deg, rms=float(np.sqrt(np.mean(rx ** 2 + ry ** 2))),
                n=int(M.shape[0]))

def apply_transform(T, xA, yA):
    u_ = (np.asarray(xA, float) - 2048.0) / 2048.0
    v_ = (np.asarray(yA, float) - 2048.0) / 2048.0
    terms = [np.ones_like(u_)]
    for p in range(T['deg'] + 1):
        for q in range(T['deg'] + 1 - p):
            if p + q == 0: continue
            terms.append(u_ ** p * v_ ** q)
    M = np.vstack(terms).T
    return M @ T['cx'], M @ T['cy']

# ---------- grid sampling ----------
def sample_pair(fA, fB, T, step=24, box=None, border=8):
    """Sample both frames at a common sky grid (grid defined in A pixels).
    Returns dict with xA,yA,xB,yB,yA,yB (y* = frame values)."""
    if box is None:
        box = (0, 4096, 0, 4096)
    x0, x1, y0, y1 = box
    gx = np.arange(x0 + border, x1 - border, step, dtype=float)
    gy = np.arange(y0 + border, y1 - border, step, dtype=float)
    GX, GY = np.meshgrid(gx, gy)
    GX = GX.ravel(); GY = GY.ravel()
    XB, YB = apply_transform(T, GX, GY)
    keep = (XB > border) & (XB < 4095 - border) & (YB > border) & (YB < 4095 - border)
    GX, GY, XB, YB = GX[keep], GY[keep], XB[keep], YB[keep]
    da = frame_data(fA); db = frame_data(fB)
    va = q3lib.sample_bilinear(da, GX, GY)
    vb = q3lib.sample_bilinear(db, XB, YB)
    m = np.isfinite(va) & np.isfinite(vb)
    return dict(xA=GX[m], yA=GY[m], xB=XB[m], yB=YB[m], vA=va[m], vB=vb[m])

# ---------- regression ----------
def bin_median(L, d, nb=30, minn=20):
    order = np.argsort(L)
    L = L[order]; d = d[order]
    idx = np.array_split(np.arange(len(L)), nb)
    lm, dm, nm = [], [], []
    for ii in idx:
        if len(ii) < minn: continue
        lm.append(np.median(L[ii])); dm.append(np.median(d[ii])); nm.append(len(ii))
    return np.array(lm), np.array(dm), np.array(nm)

def regress_alpha(L, d, nb=30, minn=20, robust=True):
    """alpha, beta from y_A - y_B = alpha*L + beta + eps (bin-median fit)."""
    lm, dm, nm = bin_median(L, d, nb, minn)
    if len(lm) < 3:
        return None
    A = np.vstack([lm, np.ones_like(lm)]).T
    w = np.sqrt(nm)
    Aw = A * w[:, None]; dw = dm * w
    sol, *_ = np.linalg.lstsq(Aw, dw, rcond=None)
    pred = A @ sol
    ss_res = np.sum((dm - pred) ** 2)
    ss_tot = np.sum((dm - dm.mean()) ** 2)
    r2 = 1 - ss_res / ss_tot if ss_tot > 0 else np.nan
    return dict(alpha=float(sol[0]), beta=float(sol[1]), r2=float(r2),
                npts=int(len(L)), nbins=len(lm), Lmin=float(lm[0]), Lmax=float(lm[-1]),
                Lmed=float(np.median(L)))
