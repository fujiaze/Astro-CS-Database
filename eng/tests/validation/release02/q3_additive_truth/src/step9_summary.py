"""Consolidate Q3 headline numbers + ASCII spatial maps + a small PNG figure
(pure numpy/zlib PNG writer; matplotlib is unavailable)."""
import sys, json, os
sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')
import numpy as np, q3lib, q3core as C
from step5_pairdetail import refined_match
from scipy.spatial import cKDTree
import zlib, struct

OUT = 'run/RELEASE-02/q3-additive-truth/data'
os.makedirs(OUT, exist_ok=True)

def write_png(path, rgb):
    h, w, _ = rgb.shape
    raw = b''.join(b'\x00' + rgb[i].tobytes() for i in range(h))
    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 6))
    png += chunk(b'IEND', b'')
    open(path, 'wb').write(png)

def star_scalar_fit(P):
    sky = sorted({p['A'] for p in P} | {p['B'] for p in P})
    idx = {s: i for i, s in enumerate(sky)}; N = len(sky)
    rows = []; obs = []; w = []
    for p in P:
        r = np.zeros(N); r[idx[p['A']]] = 1; r[idx[p['B']]] = -1
        rows.append(r); obs.append(np.log(p['med'])); w.append(p['n_used'])
    A = np.array(rows)[:, 1:]; y = np.array(obs); w = np.sqrt(np.array(w, float))
    sol, *_ = np.linalg.lstsq(A * w[:, None], y * w, rcond=None)
    g = np.concatenate([[0.0], sol]); resid = y - A @ sol
    return sky, g, float(np.std(resid)), float(np.percentile(np.abs(resid), 90))

def block_map(fA, fB, blocks=3):
    T, m = refined_match(fA, fB)
    fa = np.asarray(m['fA'], float); fb = np.asarray(m['fB'], float)
    xa = np.asarray(m['xA'], float); ya = np.asarray(m['yA'], float)
    r = fa / fb; g = np.isfinite(r) & (r > 0)
    r = r[g]; xa = xa[g]; ya = ya[g]
    bm = np.full((blocks, blocks), np.nan); bc = np.zeros((blocks, blocks), int)
    for i in range(blocks):
        for j in range(blocks):
            k = (xa >= j * 4096 / blocks) & (xa < (j + 1) * 4096 / blocks) & \
                (ya >= i * 4096 / blocks) & (ya < (i + 1) * 4096 / blocks)
            bc[i, j] = k.sum()
            if k.sum() > 20: bm[i, j] = np.median(r[k])
    return bm, bc, float(np.median(r))

def ascii_map(M, title):
    lines = [title]
    for row in M:
        lines.append('   ' + ' '.join('%7.4f' % v if np.isfinite(v) else '   --  ' for v in row))
    return '\n'.join(lines)

if __name__ == '__main__':
    P = json.load(open(OUT + '/pairs_wcs.json'))
    P = [p for p in P if p['n_used'] >= 50]
    sky, g, rms, p90 = star_scalar_fit(P)
    print('=== per-frame scalar fit (%d pairs, %d frames) ===' % (len(P), len(sky)))
    print('residual RMS(log) = %.5f (%.3f%%)   p90 = %.5f' % (rms, 100 * rms, p90))
    order = np.argsort(g)
    for k in order:
        print('   %-56s g=%+.4f (x%.3f)' % (sky[k][:56], g[k], np.exp(g[k])))
    print()
    print('=== a_AB by class ===')
    import collections
    def cls(p):
        if p['same_grp']: return 'same-group'
        if p['same_panel']: return 'cross-tel same-panel'
        if p['adjacent']: return 'adjacent-panel (SEAM)'
        return 'non-overlap'
    for cl in ['same-group', 'cross-tel same-panel', 'adjacent-panel (SEAM)', 'non-overlap']:
        sub = [p for p in P if cls(p) == cl]
        if not sub: continue
        a = np.array([p['med'] for p in sub])
        print('   %-24s n=%3d  a_AB median=%.4f  |a-1| median=%.4f  range %.3f..%.3f' % (
            cl, len(sub), np.median(a), np.median(np.abs(a - 1)), a.min(), a.max()))
    print()
    reg = q3lib.registry()
    def find(p, t, d): return [x for x in reg if x['panel'] == p and x['tel'] == t and x['date'] == d][0]
    maps = {}
    cases = [('M1', 'T2', '20251212', 'M1', 'T2', '20251224', 'same-tel M1'),
             ('M2', 'T2', '20251212', 'M2', 'T2', '20251224', 'same-tel M2'),
             ('M5', 'T2', '20251212', 'M5', 'T2', '20251224', 'same-tel M5'),
             ('M1', 'T2', '20251212', 'M1', 'T3', '20251126', 'CROSS-tel M1'),
             ('M2', 'T2', '20251212', 'M2', 'T3', '20251128', 'CROSS-tel M2')]
    print('=== 3x3 spatial ratio maps (rows = y, cols = x) ===')
    for ca, cb, tag in [(c[:3], c[3:6], c[6]) for c in cases]:
        fA = find(*ca); fB = find(*cb)
        bm, bc, med = block_map(fA, fB)
        maps[tag] = dict(map=bm.tolist(), cnt=bc.tolist(), med=med)
        print(ascii_map(bm, '%s  (median a=%.4f, 3x3 p2p=%.2f%%)' % (tag, med, (np.nanmax(bm) - np.nanmin(bm)) * 100)))
    # figure: 2 panels of 3x3 maps as heat maps
    def heat(M, scale):
        h = np.zeros((M.shape[0] * 40, M.shape[1] * 40, 3), np.uint8)
        v = (M - 1) / scale
        for i in range(M.shape[0]):
            for j in range(M.shape[1]):
                c = int(np.clip(128 + 127 * np.tanh(v[i, j]), 0, 255))
                col = np.array([c, 128, 255 - c], np.uint8)
                h[i * 40:(i + 1) * 40, j * 40:(j + 1) * 40] = col
        return h
    h1 = heat(np.array(maps['same-tel M1']['map']), 0.02)
    h2 = heat(np.array(maps['CROSS-tel M1']['map']), 0.02)
    write_png('run/RELEASE-02/q3-additive-truth/data/fig_spatial_ratio.png', np.concatenate([h1, h2], axis=1))
    json.dump(dict(scalar_fit=dict(rms=rms, p90=p90, g={sky[i]: float(g[i]) for i in range(len(sky))}),
                   maps=maps), open(OUT + '/summary.json', 'w'), indent=1)
    print('\nWROTE summary.json + fig_spatial_ratio.png')
