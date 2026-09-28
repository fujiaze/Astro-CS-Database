#!/usr/bin/env python3
"""COMPRESS-01: HiPS 2.0 WD §4.3.2 "Trim reduction" 在真实产物上的收益测量.
Trim = 去掉 FITS tile 中全 NaN/0 的外围边距, 用 TRIM1/TRIM2/ONAXIS1/ONAXIS2 记录.
本脚本按 signal/support 的"有效域包围盒"计算 trim 后的数据段字节数.
（只读; 不实现 trim, 只量化收益。）
"""
import glob, json, os, sys
import numpy as np

ROOT = sys.argv[1] if len(sys.argv) > 1 else "run/PERF-401/out/real16_w1"
OUT = sys.argv[2] if len(sys.argv) > 2 else "run/COMPRESS-01/evidence/trim_scan.json"
HDR, NX, NY = 2880, 512, 512

rows = []
for layer in ["signal", "support"]:
    for order in range(0, 10):
        for f in sorted(glob.glob(os.path.join(ROOT, layer, "Norder%d" % order, "**", "*.fits"), recursive=True)):
            if not os.path.basename(f).startswith("Npix"):
                continue
            if os.path.getsize(f) < HDR + NX * NY * 4:
                continue
            with open(f, "rb") as g:
                g.seek(HDR); buf = g.read(NX * NY * 4)
            a = np.frombuffer(buf, dtype=">f4").reshape(NY, NX)
            valid = np.isfinite(a) if layer == "signal" else (a != 0)
            if not valid.any():
                rows.append(dict(layer=layer, order=order, file=os.path.relpath(f, ROOT),
                                 trim_bytes=0, full_bytes=NX * NY * 4, valid=0, bbox=[0, 0, 0, 0]))
                continue
            ys, xs = np.nonzero(valid)
            y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
            tw, th = int(x1 - x0 + 1), int(y1 - y0 + 1)
            rows.append(dict(layer=layer, order=order, file=os.path.relpath(f, ROOT),
                             trim_bytes=tw * th * 4, full_bytes=NX * NY * 4,
                             valid=int(valid.sum()), bbox=[int(x0), int(y0), int(tw), int(th)]))
json.dump(rows, open(OUT, "w"), indent=1)
full = sum(r["full_bytes"] for r in rows); trim = sum(r["trim_bytes"] for r in rows)
print("瓦片 %d  signal+support" % len(rows))
print("trim 前数据段 %.1f MiB -> trim 后 %.1f MiB  = %.4f  (省 %.1f%%)" % (
    full / 2**20, trim / 2**20, trim / full, 100 * (1 - trim / full)))
for layer in ["signal", "support"]:
    sub = [r for r in rows if r["layer"] == layer]
    fz = sum(r["full_bytes"] for r in sub); tz = sum(r["trim_bytes"] for r in sub)
    print("  %-8s %.4f" % (layer, tz / fz))
print()
print("%-8s %5s %5s %10s %10s" % ("layer", "order", "n", "trim/full", "中位 bbox 面积比"))
for layer in ["signal", "support"]:
    for order in range(0, 10):
        sub = [r for r in rows if r["layer"] == layer and r["order"] == order]
        if not sub: continue
        fz = sum(r["full_bytes"] for r in sub); tz = sum(r["trim_bytes"] for r in sub)
        med = float(np.median([r["trim_bytes"] / r["full_bytes"] for r in sub]))
        print("%-8s %5d %5d %10.4f %10.4f" % (layer, order, len(sub), tz / fz, med))
