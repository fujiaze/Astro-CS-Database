#!/usr/bin/env python3
# V10 只读：定点导出候选站点上下文（不改任何文件）
import subprocess
targets = [
 ("lib/gaia_xpsd_client/src/gaia_client.c", 855, 880),
 ("lib/astro_image_io/src/hips/aio_hips_writer.cpp", 240, 268),
 ("lib/phase2/src/sampler.cpp", 118, 140),
 ("lib/orchestrator/cpp/src/checkpoint.cpp", 155, 205),
 ("lib/astro_image_io/src/aio_xisf.cpp", 526, 560),
]
for f,a,b in targets:
    print("="*20, f, a, b)
    try:
        L=open(f,encoding="utf-8",errors="replace").read().splitlines()
    except Exception as e:
        print("READFAIL",e); continue
    for i in range(a, min(b, len(L))+1):
        print("%5d: %s" % (i, L[i-1]))
