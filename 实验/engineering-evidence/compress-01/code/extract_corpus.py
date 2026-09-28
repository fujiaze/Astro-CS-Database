#!/usr/bin/env python3
"""COMPRESS-01 corpus 提取: 从真实 HiPS 产物中抽取**稠密数据层**瓦片载荷.

只读打开真实产物 (run/PERF-401/out/real16_w1), 不改动任何字节.
输出到 run/COMPRESS-01/data/:
  <layer>_<order>_<ipix>.bin        原样 FITS 数据段 (big-endian f32, 512*512*4 B)
  <layer>_<order>_<ipix>.dense.bin  仅有效载荷 (finite 且非零/非 NaN) 的 little-endian f32
  controls: zeros.bin / rand.bin / const.bin / f64noise.bin
"""
import glob, json, os, sys
import numpy as np
from astropy.io import fits

ROOT = sys.argv[1] if len(sys.argv) > 1 else "run/PERF-401/out/real16_w1"
OUT = sys.argv[2] if len(sys.argv) > 2 else "run/COMPRESS-01/data"
os.makedirs(OUT, exist_ok=True)

LAYERS = ["signal", "variance", "ivar", "support"]
LEAF_ORDER = 9
N_LEAF_PER_LAYER = 24   # 叶级样本数 (每层)
N_HIER_PER_ORDER = 3    # 每个层级阶的样本数

def tiles(layer, order):
    fs = glob.glob(os.path.join(ROOT, layer, "Norder%d" % order, "**", "*.fits"), recursive=True)
    out = []
    for f in fs:
        b = os.path.basename(f)
        if b.startswith("Npix"):
            out.append((int(b[4:-5]), f))
    return sorted(out)

manifest = []
for layer in LAYERS:
    # 叶级: 按 ipix 等距采样, 覆盖不同 shard
    lt = tiles(layer, LEAF_ORDER)
    if lt:
        idx = np.linspace(0, len(lt) - 1, min(N_LEAF_PER_LAYER, len(lt))).astype(int)
        for i in idx:
            ipix, f = lt[i]
            with fits.open(f, memmap=True) as h:
                arr = np.asarray(h[0].data)
                hdr = {k: h[0].header[k] for k in ("BITPIX", "NAXIS1", "NAXIS2") if k in h[0].header}
            raw = arr.astype(">f4", copy=False).tobytes() if arr.dtype.byteorder == ">" else arr.astype(">f4").tobytes()
            tag = "%s_leaf_%d" % (layer, ipix)
            open(os.path.join(OUT, tag + ".bin"), "wb").write(raw)
            # dense payload: finite 且 != 0 的元素 (即方案 A 真正需要存的数)
            a = np.asarray(arr, dtype=np.float32).ravel()
            m = np.isfinite(a) & (a != 0)
            dense = a[m].astype("<f4").tobytes()
            if len(dense):
                open(os.path.join(OUT, tag + ".dense.bin"), "wb").write(dense)
            manifest.append(dict(tag=tag, layer=layer, order=LEAF_ORDER, ipix=int(ipix),
                                 nbytes=len(raw), n_dense=len(dense),
                                 n_finite=int(np.isfinite(a).sum()), n_nonzero=int((a != 0).sum()),
                                 path=os.path.relpath(f, ROOT), hdr=hdr))
    # 层级: 每阶采样
    for order in range(0, LEAF_ORDER):
        ht = tiles(layer, order)
        if not ht:
            continue
        idx = np.linspace(0, len(ht) - 1, min(N_HIER_PER_ORDER, len(ht))).astype(int)
        for i in idx:
            ipix, f = ht[i]
            with fits.open(f, memmap=True) as h:
                arr = np.asarray(h[0].data)
            raw = arr.astype(">f4").tobytes()
            tag = "%s_hier%d_%d" % (layer, order, ipix)
            open(os.path.join(OUT, tag + ".bin"), "wb").write(raw)
            a = np.asarray(arr, dtype=np.float32).ravel()
            m = np.isfinite(a) & (a != 0)
            dense = a[m].astype("<f4").tobytes()
            if len(dense):
                open(os.path.join(OUT, tag + ".dense.bin"), "wb").write(dense)
            manifest.append(dict(tag=tag, layer=layer, order=order, ipix=int(ipix),
                                 nbytes=len(raw), n_dense=len(dense),
                                 n_finite=int(np.isfinite(a).sum()), n_nonzero=int((a != 0).sum()),
                                 path=os.path.relpath(f, ROOT)))

# ---- 负例 / 对照块 (判据非退化) ----
rng = np.random.default_rng(20260922)
N = 512 * 512
open(os.path.join(OUT, "ctl_zeros.bin"), "wb").write(np.zeros(N, dtype=">f4").tobytes())
open(os.path.join(OUT, "ctl_rand_f32.bin"), "wb").write(rng.standard_normal(N).astype(">f4").tobytes())
open(os.path.join(OUT, "ctl_const_nan.bin"), "wb").write(np.full(N, np.nan, dtype=">f4").tobytes())
open(os.path.join(OUT, "ctl_rand_f64.bin"), "wb").write(rng.standard_normal(N).astype("<f8").tobytes())
open(os.path.join(OUT, "ctl_zeros_f64.bin"), "wb").write(np.zeros(N, dtype="<f8").tobytes())
# 常数块 (支持面在高层级可能接近常数)
open(os.path.join(OUT, "ctl_const_f32.bin"), "wb").write(np.full(N, 0.5, dtype=">f4").tobytes())
# 可压的平滑场 (area 通道量级)
x = np.linspace(0, 1, 512)
open(os.path.join(OUT, "ctl_smooth_f32.bin"), "wb").write(np.outer(x, x).astype(">f4").tobytes())

json.dump(manifest, open(os.path.join(OUT, "manifest.json"), "w"), indent=1)
print("tiles:", len(manifest))
tot = sum(m["nbytes"] for m in manifest)
print("total bytes: %.1f MiB" % (tot / 1048576))
