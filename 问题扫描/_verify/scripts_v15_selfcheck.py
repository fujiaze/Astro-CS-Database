# -*- coding: utf-8 -*-
"""V15 只读：selfcheck 是否遍历全部注册注入名（口径：每文件注入名注册数 vs selfcheck 实际验证数）。"""
import os, re
ROOT="/workspace/Astro CS Database"
targets = [
 "lib/astro_image_io/tests/p1hips/p1hips_tests_selfcheck.cpp",
 "lib/calibration/tests/p1cal/p1cal_tests_selfcheck.cpp",
 "lib/cosmetic/tests/p1cos/p1cos_tests_selfcheck.cpp",
 "lib/dynamic_psf/tests/p1psf/p1psf_tests_selfcheck.cpp",
 "lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp",
 "lib/phase1_session/tests/p1sess/p1sess_tests_selfcheck.cpp",
 "lib/photometric_calib/tests/p1phot/p1phot_tests_selfcheck.cpp",
 "lib/snr_estimator/tests/p1noise/p1noise_tests_selfcheck.cpp",
 "lib/star_detector/tests/p1star/p1star_tests_selfcheck.cpp",
 "tests/unit/aio_abi_selfcheck.cpp",
 "tests/unit/p1wcs/p1wcs_tests_selfcheck.cpp",
]
for t in targets:
    p=os.path.join(ROOT,t)
    s=open(p,encoding="utf-8",errors="replace").read()
    loops = "for" in s and re.search(r'for\s*\([^)]*(fault|name|Fault|NAMES|names)', s) is not None
    names_inline = re.findall(r'"([a-z0-9_]+_[a-z0-9_]+)"', s)
    has_all = bool(re.search(r'ALL_NAMES|all_names|kFaultNames|fault_names', s))
    print(f"{t:70s} 遍历全部注入名={loops or has_all}  selfcheck内联名={sorted(set(names_inline))[:6]}  行数={s.count(chr(10))+1}")
PY6=None
