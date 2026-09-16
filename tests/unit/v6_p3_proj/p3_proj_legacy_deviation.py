#!/usr/bin/env python3
"""p3_proj_legacy_deviation.py — V6 前 legacy registry v1 CAR/AIT 偏差证据

对比 legacy 实现（lib/algorithms/projection/p3_projection.cpp，编入
v6_p3_proj_legacy_probe）与 astropy/WCSLIB（标准 FITS WCS Paper II）在同
CRVAL/CRPIX/CD/CTYPE 下的 pix2world：
  * CAR: legacy 用 Y=−θ（declination 反号）-> dec 全反号；
  * AIT: legacy 缺 Paper II γ 的 √2 因子 -> 天区尺度偏差 √2。
rc=0 表示偏差如预期复现（证据成立）；rc=1 表示未复现（与 finding 矛盾）。
"""
import math
import subprocess
import sys

import numpy as np

try:
    from astropy.wcs import WCS
except Exception:
    print("astropy unavailable")
    sys.exit(2)

DEG = math.pi / 180.0
CASES = [("CAR", 0.0, 0.0, 1.0, 97, 97), ("AIT", 0.0, 0.0, 1.0, 64, 64)]


def main(probe):
    any_dev = False
    for proj, ra0, dec0, scale, w, h in CASES:
        out = subprocess.run([probe, proj, repr(ra0), repr(dec0), repr(scale),
                              str(w), str(h), "5"],
                             check=True, capture_output=True, text=True)
        head = None
        rows = []
        for line in out.stdout.splitlines():
            p = line.split()
            if not p:
                continue
            kv = {t.split("=")[0]: t.split("=")[1] for t in p[1:]}
            if p[0] == "V6PROBE":
                head = kv
            elif p[0] == "ROW":
                rows.append(kv)
        wcs = WCS(naxis=2)
        wcs.wcs.ctype = ["RA---" + proj, "DEC--" + proj]
        wcs.wcs.crval = [ra0, dec0]
        wcs.wcs.crpix = [float(head["crpix1"]), float(head["crpix2"])]
        wcs.wcs.cd = [[float(head["cd11"]), float(head["cd12"])],
                      [float(head["cd21"]), float(head["cd22"])]]
        wcs.wcs.set()
        worst = 0.0
        worst_flip = 0.0
        worst_scale = 0.0
        for r in rows:
            if r["status"] != "0":
                continue
            x, y = float(r["x"]), float(r["y"])
            ra_a, dec_a = wcs.all_pix2world([[x, y]], 0)[0]
            dra = abs(ra_a - float(r["ra"]))
            dra = min(dra, 360.0 - dra)
            sec = math.hypot(dra, dec_a - float(r["dec"])) * 3600.0
            worst = max(worst, sec)
            if proj == "CAR":
                worst_flip = max(worst_flip, abs(dec_a - (-float(r["dec"]))) * 3600.0)
            if proj == "AIT":
                # 尺度: 把 legacy 给出的天球点喂给 astropy 反解，回不到同一像素
                # 即证明 legacy 天区尺度 ≠ 标准 AIT（缺 √2）
                pa = wcs.all_world2pix([[float(r["ra"]), float(r["dec"])]], 0)[0]
                worst_scale = max(worst_scale, abs(pa[0] - x), abs(pa[1] - y))
        print("%s: legacy_vs_astropy max=%.3f arcsec" % (proj, worst))
        if proj == "CAR":
            print("    CAR dec 反号残差（legacy dec vs -astropy dec）max=%.3f arcsec"
                  % worst_flip)
            any_dev = any_dev or (worst > 3600.0)
        if proj == "AIT":
            print("    AIT world->pix 尺度残差 max=%.3f px（√2 缺因子）" % worst_scale)
            any_dev = any_dev or (worst > 60.0)
    print("LEGACY-DEVIATION %s" % ("REPRODUCED" if any_dev else "NOT-REPRODUCED"))
    return 0 if any_dev else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
