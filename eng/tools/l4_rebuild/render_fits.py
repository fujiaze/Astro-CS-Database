#!/usr/bin/env python3
"""FITS -> PNG 渲染（用于视觉验收）。

要点：天文数据动态范围极大，纯线性显示基本全黑。默认用 asinh 拉伸 + 百分位黑白点。

用法:
  render_fits.py <in.fits> <out.png> [--pct 0.5,99.5] [--asinh 3.0] [--inv] [--max 2048]
  render_fits.py --grid <out.png> <in1.fits> <in2.fits> ... [--cols 2] [--title ...]
"""
import sys, os, math
import numpy as np
from PIL import Image

def load(path):
    from astropy.io import fits
    with fits.open(path, memmap=False) as h:
        for hdu in h:
            if hdu.data is None: continue
            d = np.asarray(hdu.data, dtype=np.float64)
            if d.ndim == 3: d = d[0]
            if d.ndim != 2: continue
            return d
    raise SystemExit('no 2D image data in ' + path)

def stretch(d, pct=(0.5, 99.5), asinh_k=3.0):
    fin = np.isfinite(d)
    if not fin.any(): return np.zeros(d.shape, np.uint8)
    lo, hi = np.percentile(d[fin], pct)
    if not np.isfinite(lo) or not np.isfinite(hi) or hi <= lo:
        lo, hi = float(np.nanmin(d[fin])), float(np.nanmax(d[fin])) or 1.0
    x = (d - lo) / (hi - lo)
    x = np.clip(x, 0.0, 1.0)
    # asinh 拉伸（标准天文显示）：保留暗部细节又不过曝亮部
    y = np.arcsinh(asinh_k * x) / math.asinh(asinh_k)
    y[~fin] = 0.0
    return (np.clip(y, 0, 1) * 255).astype(np.uint8)

def downscale(a, mx):
    h, w = a.shape
    if max(h, w) <= mx: return a
    s = max(h, w) / float(mx)
    nh, nw = max(1, int(h/s)), max(1, int(w/s))
    yi = (np.arange(nh) * s).astype(int).clip(0, h-1)
    xi = (np.arange(nw) * s).astype(int).clip(0, w-1)
    return a[np.ix_(yi, xi)]

def save(a, out, inv=False, mx=2048):
    a = downscale(a, mx)
    if inv: a = 255 - a
    Image.fromarray(a, mode='L').save(out)
    print('wrote', out, a.shape)

def main():
    args = sys.argv[1:]
    if not args: print(__doc__); return 2
    inv = '--inv' in args; args = [a for a in args if a != '--inv']
    pct = (0.5, 99.5); asinh_k = 3.0; mx = 2048
    if '--pct' in args:
        i = args.index('--pct'); pct = tuple(float(v) for v in args[i+1].split(',')); del args[i:i+2]
    if '--asinh' in args:
        i = args.index('--asinh'); asinh_k = float(args[i+1]); del args[i:i+2]
    if '--max' in args:
        i = args.index('--max'); mx = int(args[i+1]); del args[i:i+2]
    if args[0] == '--grid':
        out = args[1]; files = args[2:]
        cols = 2
        if '--cols' in files:
            i = files.index('--cols'); cols = int(files[i+1]); del files[i:i+2]
        imgs = [downscale(stretch(load(f), pct, asinh_k), mx) for f in files]
        if not imgs: print('no inputs'); return 2
        h = max(i.shape[0] for i in imgs); w = max(i.shape[1] for i in imgs)
        rows = (len(imgs) + cols - 1) // cols
        canvas = np.zeros((rows*h, cols*w), np.uint8)
        for k, im in enumerate(imgs):
            r, c = divmod(k, cols)
            canvas[r*h:r*h+im.shape[0], c*w:c*w+im.shape[1]] = im
        save(canvas, out, inv, mx)
        return 0
    if len(args) < 2: print(__doc__); return 2
    save(stretch(load(args[0]), pct, asinh_k), args[1], inv, mx)
    return 0

if __name__ == '__main__':
    sys.exit(main())