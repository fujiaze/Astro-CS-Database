#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PHOT-VERIFY: pixel-level fixed-aperture ratio + background (additive) measurement.

For a pair of frames (A,B) with known WCS, take stars matched by sky position,
measure a FIXED-aperture flux and local sky in both frames on the calibrated FITS,
then analyse the spatial structure of:
   multiplicative ratio  log10(F_A/F_B)
   additive offset       b_A - b_B   (local sky, ADU/px)
   joint                 F_A = a*F_B + c   (per spatial bin)
"""
import os, sys, json, math
import numpy as np
from astropy.io import fits
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pair_ratio as P

ROOT = P.ROOT
def cal_path(tile, cleaned_file):
    b = os.path.basename(cleaned_file)
    b = b.replace('cleaned_', 'calibrated_')
    return os.path.join(ROOT, 'run/RELEASE-02/L4-rebuild/norm', tile, b)

def aperture_sum(img, xs, ys, r, rin, rout, chunk=4000):
    """Vectorised fixed-aperture photometry via patch extraction."""
    H, W = img.shape
    n = len(xs)
    flux = np.full(n, np.nan); sky = np.full(n, np.nan); npix = np.zeros(n, int)
    R = int(math.ceil(rout))
    P = 2*R+1
    gy, gx = np.mgrid[-R:R+1, -R:R+1]
    d2 = gx.astype(float)**2 + gy.astype(float)**2
    ap_mask = d2 <= r*r
    an_mask = (d2 >= rin*rin) & (d2 <= rout*rout)
    for s in range(0, n, chunk):
        e = min(n, s+chunk)
        x = xs[s:e]; y = ys[s:e]
        x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int)
        ix = x0[:,None,None] + (gx[None,:,:])
        iy = y0[:,None,None] + (gy[None,:,:])
        ok = (ix >= 0) & (ix < W) & (iy >= 0) & (iy < H)
        ix = np.clip(ix, 0, W-1); iy = np.clip(iy, 0, H-1)
        patch = img[iy, ix].astype(np.float64)
        patch = np.where(ok, patch, np.nan)
        # per-star sky = median over annulus
        ann = np.where(an_mask[None,:,:], patch, np.nan)
        with np.errstate(all='ignore'):
            sk = np.nanmedian(ann, axis=(1,2))
        apv = np.where(ap_mask[None,:,:], patch, np.nan)
        with np.errstate(all='ignore'):
            fl = np.nansum(apv - sk[:,None,None], axis=(1,2))
        good = np.isfinite(fl) & np.isfinite(sk) & (np.sum(ap_mask & ok, axis=(1,2)) > 10)
        idx = np.arange(s,e)
        flux[idx[good]] = fl[good]; sky[idx[good]] = sk[good]
        npix[idx[good]] = np.sum(ap_mask & ok, axis=(1,2))[good]
    return flux, sky, npix

def measure_pair(tileA, idxA, tileB, idxB, r=6.0, rin=10.0, rout=16.0, min_flux=20000.0, tag='', max_stars=6000):
    A = P.Frame(tileA, idxA); B = P.Frame(tileB, idxB)
    i, j, dist = P.match(A, B, tol_arcsec=0.7, min_flux=min_flux, max_flux_ratio=20.0)
    if len(i) < 50:
        return None
    if max_stars and len(i) > max_stars:
        sel = np.argsort(-(A.flux[i]+B.flux[j]))[:max_stars]
        i, j, dist = i[sel], j[sel], dist[sel]
    imgA = fits.getdata(cal_path(tileA, A.file)).astype(np.float32)
    imgB = fits.getdata(cal_path(tileB, B.file)).astype(np.float32)
    fA, sA, nA = aperture_sum(imgA, A.x[i], A.y[i], r, rin, rout)
    fB, sB, nB = aperture_sum(imgB, B.x[j], B.y[j], r, rin, rout)
    ok = np.isfinite(fA) & np.isfinite(fB) & (fA > 0) & (fB > 0) & (nA > 10) & (nB > 10)
    i, j = i[ok], j[ok]; fA, fB, sA, sB = fA[ok], fB[ok], sA[ok], sB[ok]
    del imgA, imgB
    ratio = fA/fB
    z = np.log10(ratio)
    # clip
    for _ in range(3):
        med = np.median(z); s = 1.482602218505602*np.median(np.abs(z-med))
        if s <= 0: break
        keep = np.abs(z-med) < 4.0*s
        if keep.all(): break
        i,j,fA,fB,sA,sB,z,ratio = i[keep],j[keep],fA[keep],fB[keep],sA[keep],sB[keep],z[keep],ratio[keep]
    cd = math.cos(math.radians(0.5*(A.dec.mean()+B.dec.mean())))
    ra_c = 0.5*(A.ra[i]+B.ra[j]); dec_c = 0.5*(A.dec[i]+B.dec[j])
    px = (ra_c-ra_c.mean())*cd*3600.0; py = (dec_c-dec_c.mean())*3600.0
    def polyfit(z, order):
        cols=[np.ones_like(px)]
        if order>=1: cols += [px,py]
        if order>=2: cols += [px*px, px*py, py*py]
        M=np.column_stack(cols); c,*_=np.linalg.lstsq(M,z,rcond=None)
        gx=np.linspace(px.min(),px.max(),40); gy=np.linspace(py.min(),py.max(),40)
        GX,GY=np.meshgrid(gx,gy)
        cols=[np.ones_like(GX)]
        if order>=1: cols += [GX,GY]
        if order>=2: cols += [GX*GX,GX*GY,GY*GY]
        MM=np.column_stack([c_.ravel() for c_ in cols]); zz=MM@c
        return c, z-M@c, float(zz.max()-zz.min())
    res={}
    for o in (0,1,2):
        c,resid,ptp = polyfit(z,o)
        res['poly%d'%o]=dict(coef=[float(x) for x in c], resid_mad=float(1.482602218505602*np.median(np.abs(resid-np.median(resid)))),
                             resid_rms=float(np.std(resid)), model_ptp_dex=float(ptp))
    # ---- robust binned maps over the overlap (8x8 grid) ----
    nb = 8
    xb = np.linspace(px.min(), px.max(), nb+1)
    yb = np.linspace(py.min(), py.max(), nb+1)
    xi = np.clip(np.digitize(px, xb)-1, 0, nb-1)
    yi = np.clip(np.digitize(py, yb)-1, 0, nb-1)
    def binned(vals, minn=8):
        M = np.full((nb,nb), np.nan)
        for aa in range(nb):
            for bb in range(nb):
                m = (xi==aa)&(yi==bb)
                if m.sum() >= minn: M[aa,bb] = np.median(vals[m])
        return M
    def mad(v):
        v = v[np.isfinite(v)]
        if v.size == 0: return 0.0
        return float(1.482602218505602*np.median(np.abs(v-np.median(v))))
    Mr = binned(z)
    Mb = binned(sA)
    # additive joint per bin: sA = a*sB + c
    Ma = np.full((nb,nb), np.nan); Mc = np.full((nb,nb), np.nan)
    for aa in range(nb):
        for bb in range(nb):
            m = (xi==aa)&(yi==bb)
            if m.sum() >= 8:
                cc, *_ = np.linalg.lstsq(np.column_stack([sB[m], np.ones(m.sum())]), sA[m], rcond=None)
                Ma[aa,bb] = cc[0]; Mc[aa,bb] = cc[1]
    res['binmap'] = dict(
        nb=nb,
        log_ratio=Mr.tolist(), sky_A=Mb.tolist(),
        a_bg=Ma.tolist(), c_bg=Mc.tolist(),
        ratio_ptp_dex=float(np.nanmax(Mr)-np.nanmin(Mr)),
        ratio_mad_dex=mad(Mr),
        a_bg_ptp=float(np.nanmax(Ma)-np.nanmin(Ma)),
        c_bg_ptp=float(np.nanmax(Mc)-np.nanmin(Mc)),
        c_bg_mad=mad(Mc))
    # additive: local sky difference
    dsky = sA - sB
    # background per-pixel difference stats
    res['meta'] = dict(tag=tag, n=int(len(i)), ratio_median=float(np.median(ratio)),
                       sky_A_median=float(np.median(sA)), sky_B_median=float(np.median(sB)),
                       z_mad=float(1.482602218505602*np.median(np.abs(z-np.median(z)))),
                       dsky_median=float(np.median(dsky)), dsky_mad=float(1.482602218505602*np.median(np.abs(dsky-np.median(dsky)))),
                       sA_median=float(np.median(sA)), sB_median=float(np.median(sB)),
                       dist_med=float(np.median(dist)), r=r, rin=rin, rout=rout, min_flux=min_flux)
    return res, dict(px=px,py=py,z=z,fA=fA,fB=fB,sA=sA,sB=sB,ra=ra_c,dec=dec_c,Ax=A.x[i],Ay=A.y[i],Bx=B.x[j],By=B.y[j])
