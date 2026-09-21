"""Q3 ADDITIVE-TRUTH shared library.
Frame registry + solved-WCS loading (p1_wcs.json) + bilinear sampling.
Read-only: never writes outside run/RELEASE-02/q3-additive-truth/.
"""
import glob, json, os, re
import numpy as np
from astropy.io import fits
from astropy.wcs import WCS
from astropy.wcs import Sip

REPO = '/workspace/Astro CS Database'
L4 = os.path.join(REPO, 'run/RELEASE-02/L4-rebuild')

def registry():
    out = []
    for f in sorted(glob.glob(os.path.join(L4, 'norm', '*', 'calibrated_*.fts'))):
        grp = os.path.basename(os.path.dirname(f))
        base = os.path.basename(f)
        stem = base.replace('calibrated_', '').replace('.fts', '')
        # frame dir key
        key = stem.replace('@', '_')
        fdir = os.path.join(os.path.dirname(f), key)
        m = re.match(r'M42_(M\d)_(T\d)_flying_dutchman-(\d{8})@(\d{6})', stem)
        panel, tel, date, tm = m.groups()
        h = fits.getheader(f)
        out.append(dict(path=f, group=grp, stem=stem, key=key, fdir=fdir,
                        panel=panel, tel=tel, date=date, time=tm,
                        wcs_json=os.path.join(fdir, 'p1_wcs.json'),
                        phot_json=os.path.join(os.path.dirname(f), 'p1_phot.json'),
                        crval1=h.get('CRVAL1'), crval2=h.get('CRVAL2'),
                        exptime=h.get('EXPTIME'), airmass=h.get('AIRMASS'),
                        dateobs=h.get('DATE-OBS')))
    return out

def load_wcs(frame):
    """Build astropy WCS (TAN-SIP) from p1_wcs.json. 0-based array index in/out:
    all_world2pix returns 0-based (we subtract 1 from FITS xp)."""
    d = json.load(open(frame['wcs_json']))
    w = d['wcs']
    hdr = {}
    hdr['CTYPE1'] = w['ctype1']; hdr['CTYPE2'] = w['ctype2']
    hdr['CRPIX1'] = w['crpix1']; hdr['CRPIX2'] = w['crpix2']
    hdr['CRVAL1'] = w['crval1']; hdr['CRVAL2'] = w['crval2']
    hdr['CD1_1'] = w['cd11']; hdr['CD1_2'] = w['cd12']
    hdr['CD2_1'] = w['cd21']; hdr['CD2_2'] = w['cd22']
    hdr['NAXIS'] = 2; hdr['NAXIS1'] = 4096; hdr['NAXIS2'] = 4096
    hdr['RADESYS'] = 'ICRS'
    W = WCS(hdr)
    if 'sip' in w:
        s = w['sip']
        def _sq(v):
            a = np.array(v, dtype=np.float64)
            n = int(round(len(a) ** 0.5))
            return a.reshape(n, n)          # dense [p][q], p,q in 0..n-1
        W.sip = Sip(_sq(s['a']), _sq(s['b']), _sq(s['ap']), _sq(s['bp']), W.wcs.crpix)
    return W

def pix_to_sky(W, x0, y0):
    """x0,y0 = 0-based array indices. Returns RA,Dec deg."""
    ra, dec = W.all_pix2world(np.asarray(x0, dtype=np.float64),
                              np.asarray(y0, dtype=np.float64), 0)
    return ra, dec

def sky_to_pix(W, ra, dec):
    """Returns 0-based x,y array indices."""
    x, y = W.all_world2pix(np.asarray(ra, dtype=np.float64),
                           np.asarray(dec, dtype=np.float64), 0)
    return x, y

def sample_bilinear(arr, x, y):
    """Bilinear sample of 2-D array at 0-based float coords (arrays).
    Out-of-range -> NaN."""
    x = np.asarray(x, dtype=np.float64); y = np.asarray(y, dtype=np.float64)
    x0 = np.floor(x).astype(np.int64); y0 = np.floor(y).astype(np.int64)
    fx = x - x0; fy = y - y0
    H, Wd = arr.shape
    ok = (x0 >= 0) & (y0 >= 0) & (x0 < Wd - 1) & (y0 < H - 1)
    x0c = np.clip(x0, 0, Wd - 2); y0c = np.clip(y0, 0, H - 2)
    a = np.asarray(arr[y0c, x0c], dtype=np.float64)
    b = np.asarray(arr[y0c, x0c + 1], dtype=np.float64)
    c = np.asarray(arr[y0c + 1, x0c], dtype=np.float64)
    d = np.asarray(arr[y0c + 1, x0c + 1], dtype=np.float64)
    v = (a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy)
    v = np.where(ok, v, np.nan)
    return v

def load_flux(frame):
    """Per-frame source catalog (x,y,flux,snr,background) from p1_flux.json."""
    pj = os.path.join(os.path.dirname(frame['path']), 'p1_flux.json')
    d = json.load(open(pj))
    for fr in d['frames']:
        if fr['file'].replace('cleaned_', '').replace('.fts', '') == frame['stem']:
            res = fr['results']
            n = len(res)
            x = np.array([r['x'] for r in res], dtype=np.float64)
            y = np.array([r['y'] for r in res], dtype=np.float64)
            flux = np.array([r['flux'] for r in res], dtype=np.float64)
            snr = np.array([r['snr'] for r in res], dtype=np.float64)
            valid = np.array([r['valid'] for r in res], dtype=bool)
            bg = np.array([r['background'] for r in res], dtype=np.float64)
            return dict(x=x, y=y, flux=flux, snr=snr, valid=valid, background=bg)
    raise KeyError('frame not in flux json: ' + frame['stem'])
