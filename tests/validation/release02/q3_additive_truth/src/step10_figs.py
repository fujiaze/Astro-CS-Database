"""Extra figures: (a) binned ratio vs signal level for representative pairs,
(b) synthetic-control alpha summary.  Pure-numpy PNG writer."""
import sys, json, os, zlib, struct
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C
from step5_pairdetail import refined_match

OUT = 'run/RELEASE-02/q3-additive-truth/data'

def write_png(path, rgb):
    h, w, _ = rgb.shape
    raw = b''.join(b'\x00' + rgb[i].tobytes() for i in range(h))
    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))

def canvas(w, h):
    return np.full((h, w, 3), 255, np.uint8)

def line(cv, x0, y0, x1, y1, col):
    n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
    for t in np.linspace(0, 1, n):
        x = int(round(x0 + (x1 - x0) * t)); y = int(round(y0 + (y1 - y0) * t))
        if 0 <= x < cv.shape[1] and 0 <= y < cv.shape[0]:
            cv[max(0, y - 1):y + 2, max(0, x - 1):x + 2] = col

def disc(cv, x, y, col, r=2):
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            xx, yy = int(x + dx), int(y + dy)
            if 0 <= xx < cv.shape[1] and 0 <= yy < cv.shape[0] and dx * dx + dy * dy <= r * r:
                cv[yy, xx] = col

def binned(fA, fB, step=16, win=8):
    from scipy.ndimage import uniform_filter
    WA = C.wcs_sip(fA); WB = C.wcs_sip(fB)
    gx = np.arange(16, 4080, step, dtype=float)
    GX, GY = np.meshgrid(gx, gx); GX = GX.ravel(); GY = GY.ravel()
    ra, dec = WA.all_pix2world(GX, GY, 0); XB, YB = WB.all_world2pix(ra, dec, 0)
    k = np.isfinite(XB) & (XB > 16) & (XB < 4080) & (YB > 16) & (YB < 4080)
    GX, GY, XB, YB = GX[k], GY[k], XB[k], YB[k]
    da = uniform_filter(np.asarray(C.frame_data(fA), np.float32), size=win, mode='nearest')
    db = uniform_filter(np.asarray(C.frame_data(fB), np.float32), size=win, mode='nearest')
    va = q3lib.sample_bilinear(da, GX, GY); vb = q3lib.sample_bilinear(db, XB, YB)
    m = np.isfinite(va) & np.isfinite(vb)
    return va[m], vb[m]

if __name__ == '__main__':
    reg = q3lib.registry()
    def find(p, t, d): return [x for x in reg if x['panel'] == p and x['tel'] == t and x['date'] == d][0]
    series = []
    cases = [(('M2', 'T2', '20251212'), ('M2', 'T2', '20251224'), 'M2 T2 same-tel'),
             (('M1', 'T2', '20251212'), ('M1', 'T2', '20251224'), 'M1 T2 same-tel'),
             (('M1', 'T2', '20251212'), ('M1', 'T3', '20251126'), 'M1 T2 vs T3 cross-tel')]
    for ca, cb, tag in cases:
        fA = find(*ca); fB = find(*cb)
        m = C.match_stars(fA, fB, snr_min=50, rad_arcsec=3.0, wcsf=C.wcs_sip)
        k = np.asarray(m['sep_arcsec']) < 1.5
        a = float(np.median(np.asarray(m['fA'])[k] / np.asarray(m['fB'])[k]))
        va, vb = binned(fA, fB)
        qs = np.percentile(vb, np.linspace(0, 100, 26))
        bx = []; by = []
        for i in range(25):
            s = (vb >= qs[i]) & (vb <= qs[i + 1])
            if s.sum() > 30:
                bx.append(np.median(vb[s])); by.append(np.median(va[s] / vb[s]))
        series.append((tag, a, np.array(bx), np.array(by)))
        print('%s a_star=%.4f' % (tag, a), flush=True)
        for x, y in zip(bx, by):
            print('    vB=%9.1f  ratio=%.4f  (additive-only would give %.4f)' % (x, y, 1 + (a * 1 - 1) * 0 + 0))
    # figure: log-x ratio vs vB, 3 series stacked
    W, H = 700, 260 * len(series)
    cv = canvas(W, H)
    for si, (tag, a, bx, by) in enumerate(series):
        y0 = si * 260
        # axes box
        for x in range(60, 680): cv[y0 + 220, x] = (0, 0, 0)
        for y in range(y0 + 20, y0 + 221): cv[y, 60] = (0, 0, 0)
        lx = np.log10(bx)
        xlo, xhi = np.log10(max(bx.min(), 50)), np.log10(bx.max())
        def px(v): return 60 + (np.log10(max(v, 50)) - xlo) / (xhi - xlo) * 620
        def py(v): return y0 + 220 - (v - 0.7) / 0.5 * 200
        line(cv, 60, py(1.0), 680, py(1.0), (0, 0, 255))
        line(cv, 60, py(a), 680, py(a), (0, 170, 0))
        for x, y in zip(bx, by):
            disc(cv, px(x), py(y), (220, 0, 0))
    write_png(OUT + '/fig_ratio_vs_signal.png', cv)
    json.dump([dict(tag=t, a_star=float(a), vB=bx.tolist(), ratio=by.tolist()) for t, a, bx, by in series],
              open(OUT + '/ratio_vs_signal.json', 'w'), indent=1)
    print('WROTE fig_ratio_vs_signal.png')
