#!/usr/bin/env python3
"""COMPRESS-01: python 侧对照基准 (zlib / lzma / zstandard), 与 C 侧同语料.
输出 evidence/py_codecs.csv: file,codec,orig,comp,ratio,c_ms,d_ms
计时: 每档 1 次预热 + reps 次取中位.
"""
import glob, lzma, os, statistics, sys, time, zlib
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pylib"))
import zstandard as zstd

DATA = sys.argv[1] if len(sys.argv) > 1 else "run/COMPRESS-01/data"
OUT = sys.argv[2] if len(sys.argv) > 2 else "run/COMPRESS-01/evidence/py_codecs.csv"
REPS_C, REPS_D = 3, 5
# 只取"真实整瓦片"数据段 (排除 .dense.bin 派生载荷与对照块), 与 zstd/FITS 主口径一致
files = sorted(f for f in glob.glob(os.path.join(DATA, "*.bin"))
               if ".dense." not in f and not os.path.basename(f).startswith("ctl_"))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
f = open(OUT, "w")
f.write("file,codec,orig,comp,ratio,c_ms,d_ms,c_mbps,d_mbps\n")
for path in files:
    buf = open(path, "rb").read()
    n = len(buf)
    base = os.path.basename(path)
    for name, comp, decomp in (
        ("zlib9", lambda b: zlib.compress(b, 9), zlib.decompress),
        ("zlib1", lambda b: zlib.compress(b, 1), zlib.decompress),
        ("lzma", lambda b: lzma.compress(b, preset=6), lzma.decompress),
        ("zstd3_py", zstd.ZstdCompressor(level=3).compress, zstd.ZstdDecompressor().decompress),
    ):
        cb = comp(buf)
        tc, td = [], []
        for _ in range(REPS_C):
            t0 = time.perf_counter(); comp(buf); tc.append(time.perf_counter() - t0)
        for _ in range(REPS_D):
            t0 = time.perf_counter(); decomp(cb); td.append(time.perf_counter() - t0)
        c = statistics.median(tc) * 1e3; d = statistics.median(td) * 1e3
        f.write("%s,%s,%d,%d,%.6f,%.4f,%.4f,%.3f,%.3f\n" % (
            base, name, n, len(cb), len(cb) / n, c, d,
            (n / 1048576.0) / (c / 1000.0), (n / 1048576.0) / (d / 1000.0)))
        f.flush()
f.close()
print("wrote", OUT, "files", len(files))