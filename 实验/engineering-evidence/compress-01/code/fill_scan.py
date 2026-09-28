#!/usr/bin/env python3
"""COMPRESS-01: 真实 HiPS 产物**全部**瓦片的填充率扫描 (只读, 逐字节读数据段).
目的: 刻画"稠密数据层"到底有多稀疏 -> 决定压缩率上界的结构性原因.
"""
import glob, json, os, sys
import numpy as np

ROOT = sys.argv[1] if len(sys.argv) > 1 else "run/PERF-401/out/real16_w1"
OUT = sys.argv[2] if len(sys.argv) > 2 else "run/COMPRESS-01/evidence/fill_scan.json"
HDR = 2880
NPIX = 512 * 512

rows = []
for layer in ["signal", "variance", "ivar", "support"]:
    for order in range(0, 10):
        fs = sorted(glob.glob(os.path.join(ROOT, layer, "Norder%d" % order, "**", "*.fits"), recursive=True))
        fs = [f for f in fs if os.path.basename(f).startswith("Npix")]
        for f in fs:
            sz = os.path.getsize(f)
            if sz < HDR + NPIX * 4:
                rows.append(dict(layer=layer, order=order, file=os.path.relpath(f, ROOT), size=sz,
                                 note="no_full_data_segment"))
                continue
            with open(f, "rb") as g:
                g.seek(HDR)
                buf = g.read(NPIX * 4)
            a = np.frombuffer(buf, dtype=">f4")
            nan = int(np.isnan(a).sum())
            zero = int((a == 0).sum())
            fin = NPIX - nan
            rows.append(dict(layer=layer, order=order, file=os.path.relpath(f, ROOT), size=sz,
                             nan=nan, zero=zero, finite=fin,
                             nan_frac=nan / NPIX, zero_frac=zero / NPIX, finite_frac=fin / NPIX,
                             distinct_bytes=int(len(np.unique(buf[::4])))))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(rows, open(OUT, "w"), indent=1)

print("%-9s %5s %6s %10s %10s %10s" % ("layer", "order", "n", "nan_frac", "zero_frac", "finite_frac"))
for layer in ["signal", "variance", "ivar", "support"]:
    for order in range(0, 10):
        sub = [r for r in rows if r["layer"] == layer and r["order"] == order and "nan_frac" in r]
        if not sub: continue
        print("%-9s %5d %6d %10.4f %10.4f %10.4f" % (
            layer, order, len(sub),
            np.mean([r["nan_frac"] for r in sub]),
            np.mean([r["zero_frac"] for r in sub]),
            np.mean([r["finite_frac"] for r in sub])))
tot = [r for r in rows if "nan_frac" in r]
print("\n合计瓦片 %d (另 %d 个无完整数据段)" % (len(tot), len(rows) - len(tot)))
for layer in ["signal", "variance", "ivar", "support"]:
    sub = [r for r in tot if r["layer"] == layer]
    if sub:
        print("  %-9s n=%3d  nan_frac=%.4f  zero_frac=%.4f  finite_frac=%.4f" % (
            layer, len(sub), np.mean([r["nan_frac"] for r in sub]),
            np.mean([r["zero_frac"] for r in sub]), np.mean([r["finite_frac"] for r in sub])))
