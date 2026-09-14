# -*- coding: utf-8 -*-
"""V15 只读：注入点注册总数 vs selfcheck 实际遍历数（口径：CHECK 宏第三参 / registry 表项）。"""
import os, re
ROOT="/workspace/Astro CS Database"
mods = {
 "p1noise": ["lib/snr_estimator/tests/p1noise"],
 "p1hips": ["lib/astro_image_io/tests/p1hips"],
 "p1cal": ["lib/calibration/tests/p1cal"],
 "p1cos": ["lib/cosmetic/tests/p1cos"],
 "p1psf": ["lib/dynamic_psf/tests/p1psf"],
 "p1drz": ["lib/healpix_db/healpix_drizzle/tests/p1drz"],
 "p1sess": ["lib/phase1_session/tests/p1sess"],
 "p1phot": ["lib/photometric_calib/tests/p1phot"],
 "p1star": ["lib/star_detector/tests/p1star"],
 "p1wcs": ["tests/unit/p1wcs"],
 "aio_abi": ["tests/unit"],
 "p2hips": ["lib/astro_image_io/tests/p2hips"],
}
for m, dirs in mods.items():
    reg=set(); inj_call=0
    for d in dirs:
        dp=os.path.join(ROOT,d)
        if not os.path.isdir(dp): continue
        for fn in os.listdir(dp):
            if not fn.endswith((".cpp",".hpp")): continue
            t=open(os.path.join(dp,fn),encoding="utf-8",errors="replace").read()
            # 注册表项: {"name", ...} 或 宏第三参 "name"
            for mm in re.finditer(r'CHECK\w*\([^;]*?,\s*"([a-z][a-z0-9_]{2,})"\s*[,)]', t):
                reg.add((fn,mm.group(1)))
            for mm in re.finditer(r'FaultRegistry|fault_registry|kFaults\s*[=\[]|\{\s*"([a-z0-9_]+_[a-z0-9_]+)"', t):
                if mm.group(1): reg.add((fn,mm.group(1)))
    sc = [x for x in reg if any("selfcheck" in f for f,_ in [ (x[0],0) ])  ]
    print(f"{m:9s} 疑似注入名注册点={len(reg):4d}  样例={sorted({n for _,n in reg})[:8]}")
PY7=None
