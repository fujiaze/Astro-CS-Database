#!/usr/bin/env python3
"""生成 zstd/fits 基准的输入清单。
 - data/*.bin              : 512x512 f32 原始数据段（用于 fits_bench）
 - files_real_fits.txt     : 真实产物中的整 .fits 文件（只读），用于 zstd 整文件口径
"""
import glob, os, random, sys
ROOT = sys.argv[1] if len(sys.argv) > 1 else "run/PERF-401/out/real16_w1"
OUT = sys.argv[2] if len(sys.argv) > 2 else "run/COMPRESS-01"
random.seed(20260922)
lines = []
for layer in ["signal", "variance", "ivar", "support"]:
    for order in range(0, 10):
        fs = sorted(glob.glob(os.path.join(ROOT, layer, "Norder%d" % order, "**", "*.fits"), recursive=True))
        fs = [f for f in fs if os.path.basename(f).startswith("Npix")]
        if not fs: continue
        k = min(3, len(fs))
        lines += random.sample(fs, k)
# 另加一批叶级
for layer in ["signal", "variance", "ivar", "support"]:
    fs = sorted(glob.glob(os.path.join(ROOT, layer, "Norder9", "**", "*.fits"), recursive=True))
    fs = [f for f in fs if os.path.basename(f).startswith("Npix")]
    lines += random.sample(fs, min(12, len(fs)))
# 路径写成相对 run/COMPRESS-01 (基准程序的工作目录)
lines = ["../../" + l for l in lines]
open(os.path.join(OUT, "files_real_fits.txt"), "w").write("\n".join(lines) + "\n")
print("real fits files:", len(lines))