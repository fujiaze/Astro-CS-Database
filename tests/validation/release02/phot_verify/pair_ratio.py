#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PHOT-VERIFY: cross-frame flux-ratio (spatial multiplicative residual) measurement.

Read-only. Uses per-frame p1_sources.json (PSF flux, background-subtracted) and
p1_wcs.json (TAN+SIP, ported in wcs_lib.py). Matches sources between two frames by
sky position, then analyses log10(flux_A/flux_B) vs position.
"""
import json, os, sys, math, glob
import numpy as np
from scipy.spatial import cKDTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wcs_lib import Wcs

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NORM = os.path.join(ROOT, 'run/RELEASE-02/L4-rebuild/norm')

def frame_dir(tile, cleaned_file):
    b = os.path.basename(cleaned_file)
    if b.endswith('.fts'): b = b[:-4]
    if b.startswith('cleaned_'): b = b[len('cleaned_'):]
    return os.path.join(NORM, tile, b.replace('@', '_'))

class Frame:
    def __init__(self, tile, idx):
        sj = json.load(open(os.path.join(NORM, tile, 'p1_sources.json')))
        fr = sj['frames'][idx]
        self.tile = tile
        self.file = fr['file']
        self.dir = frame_dir(tile, fr['file'])
        wj = json.load(open(os.path.join(self.dir, 'p1_wcs.json')))
        self.wcs = Wcs(wj)
        s = fr['sources']
        self.x = np.array([q['x'] for q in s])
        self.y = np.array([q['y'] for q in s])
        self.flux = np.array([q['flux'] for q in s])
        self.qual = np.array([q.get('quality', 0) for q in s])
        self.snr = np.array([q.get('snr', 0.0) for q in s])
        self.ra, self.dec = self.wcs.pixel_to_sky(self.x, self.y)
        self.label = os.path.basename(self.dir)

def match(A, B, tol_arcsec=0.7, min_flux=5000.0, max_flux_ratio=20.0):
    # match on (ra*cos(dec), dec) in arcsec
    cd = math.cos(math.radians(0.5*(A.dec.mean()+B.dec.mean())))
    pa = np.column_stack([A.ra*cd*3600.0, A.dec*3600.0])
    pb = np.column_stack([B.ra*cd*3600.0, B.dec*3600.0])
    tree = cKDTree(pb)
    d, j = tree.query(pa, k=1)
    ok = d < tol_arcsec
    i = np.where(ok)[0]
    j = j[ok]
    fa = A.flux[i]; fb = B.flux[j]
    good = (fa > min_flux) & (fb > min_flux) & np.isfinite(fa) & np.isfinite(fb)
    good &= (A.qual[i] == 0) & (B.qual[j] == 0)   # 0=normal (1=saturated, 2=edge)
    ratio = fa[good]/fb[good]
    good2 = (ratio > 1.0/max_flux_ratio) & (ratio < max_flux_ratio)
    i = i[good][good2]; j = j[good][good2]
    return i, j, d[ok][good][good2]

def robust_sigma(v):
    v = np.asarray(v, float)
    if v.size < 3: return 0.0
    med = np.median(v)
    return 1.482602218505602*np.median(np.abs(v-med))

def fit_poly(px, py, z, order):
    # px,py in arcsec; z in dex
    cols = [np.ones_like(px)]
    if order >= 1:
        cols += [px, py]
    if order >= 2:
        cols += [px*px, px*py, py*py]
    M = np.column_stack(cols)
    coef, *_ = np.linalg.lstsq(M, z, rcond=None)
    return coef, M

def model_ptp(coef, order, px, py):
    x0, x1 = px.min(), px.max(); y0, y1 = py.min(), py.max()
    gx = np.linspace(x0, x1, 40); gy = np.linspace(y0, y1, 40)
    GX, GY = np.meshgrid(gx, gy)
    cols = [np.ones_like(GX)]
    if order >= 1: cols += [GX, GY]
    if order >= 2: cols += [GX*GX, GX*GY, GY*GY]
    M = np.column_stack([c.ravel() for c in cols])
    z = M @ coef
    return float(z.max()-z.min()), (GX, GY, z.reshape(GX.shape))

def analyse(A, B, clip=True, **kw):
    i, j, dist = match(A, B, **kw)
    if len(i) < 20:
        return None
    # robust outlier clipping on log-ratio (blends, variables, cosmic rays)
    ratio = A.flux[i]/B.flux[j]
    z = np.log10(ratio)
    if clip:
        for _ in range(3):
            med = np.median(z); s = 1.482602218505602*np.median(np.abs(z-med))
            if s <= 0: break
            keep = np.abs(z-med) < 4.0*s
            if keep.all(): break
            i, j, dist, z = i[keep], j[keep], dist[keep], z[keep]
            ratio = ratio[keep]
    out_n_clip = int(len(i))
    # common sky position -> offsets (arcsec) from matched-pair centroid
    cd = math.cos(math.radians(0.5*(A.dec.mean()+B.dec.mean())))
    ra_c = 0.5*(A.ra[i]+B.ra[j]); dec_c = 0.5*(A.dec[i]+B.dec[j])
    px = (ra_c - ra_c.mean())*cd*3600.0
    py = (dec_c - dec_c.mean())*3600.0
    out = {
        'A': A.label, 'B': B.label, 'n_match': int(len(i)),
        'dist_med_arcsec': float(np.median(dist)),
        'z_median_dex': float(np.median(z)),
        'z_std_dex': float(robust_sigma(z)),
        'z_std_mag': float(2.5*robust_sigma(z)),
        'ratio_median': float(np.median(ratio)),
        'overlap_px_arcsec': [float(px.max()-px.min()), float(py.max()-py.min())],
        'A_x_range': [float(A.x[i].min()), float(A.x[i].max())],
        'A_y_range': [float(A.y[i].min()), float(A.y[i].max())],
        'B_x_range': [float(B.x[j].min()), float(B.x[j].max())],
        'B_y_range': [float(B.y[j].min()), float(B.y[j].max())],
    }
    for order in (0, 1, 2):
        coef, M = fit_poly(px, py, z, order)
        resid = z - M @ coef
        ptp, _ = model_ptp(coef, order, px, py)
        out['poly%d' % order] = {
            'coef': [float(c) for c in coef],
            'resid_rms_dex': float(np.std(resid)),
            'resid_rms_mag': float(2.5*np.std(resid)),
            'resid_mad_dex': float(robust_sigma(resid)),
            'resid_mad_mag': float(2.5*robust_sigma(resid)),
            'model_ptp_dex': float(ptp),
            'model_ptp_mag': float(2.5*ptp),
        }
    # ---- radial (vignetting-type) model: z = c0 + k*(rA^2 - rB^2) ----
    cxa, cya = 2047.5, 2047.5
    cxb, cyb = 2047.5, 2047.5
    rA2 = (A.x[i]-cxa)**2 + (A.y[i]-cya)**2
    rB2 = (B.x[j]-cxb)**2 + (B.y[j]-cyb)**2
    Mr = np.column_stack([np.ones_like(z), (rA2-rB2)/1e6])
    cr, *_ = np.linalg.lstsq(Mr, z, rcond=None)
    rr = z - Mr @ cr
    out['radial'] = {
        'coef': [float(c) for c in cr],
        'resid_rms_dex': float(np.std(rr)),
        'resid_mad_dex': float(robust_sigma(rr)),
        'resid_mad_mag': float(2.5*robust_sigma(rr)),
        'rA2_minus_rB2_span_1e6px2': float(((rA2-rB2)/1e6).max()-((rA2-rB2)/1e6).min()),
    }
    out['_arrays'] = dict(px=px, py=py, z=z, rA2=rA2, rB2=rB2,
                          Ax=A.x[i], Ay=A.y[i], Bx=B.x[j], By=B.y[j],
                          ra=ra_c, dec=dec_c, ratio=ratio)
    return out
