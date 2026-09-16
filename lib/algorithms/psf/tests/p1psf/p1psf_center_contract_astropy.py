#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""p1psf_center_contract_astropy.py — G-P1-CENTER-CONTRACT-1 的 astropy 第三方判定。

自包含: 运行 C++ 门 (P1PSF_CENTER_GATE_BIN) 的 --export, 用 astropy.wcs
(TAN+CD, CRPIX **1-based** per FITS WCS Paper I §2.1.1) 独立复算, 判定:

  * photometry_pixelToSky_pt0 : psf 块坐标 (index-is-center) 经
    pc::WcsTransform 的输出 == astropy(0-based 同值)          |Δ| <= 1e-6 px
  * snr_control_point_pt0     : 同上 (SNR 控制点)
  * wcs_tan_bridged_pt0       : WcsTan 单次 +1 桥接输出 == astropy(0-based 同值)
  * wcs_tan_unbridged_pt0     : 未桥接读法 == 1px 系统偏差 ⇒ 与桥接读法相差
    >= 0.9 px (门对原点平移有鉴别力, 非恒真)

fail-closed: astropy 不可用 / 门二进制缺失 / 导出缺失 ⇒ rc=2 (判红)。
用法: P1PSF_CENTER_GATE_BIN=<path> python3 p1psf_center_contract_astropy.py
"""
import json
import math
import os
import subprocess
import sys
import tempfile

try:
    import astropy
    from astropy.wcs import WCS
except Exception as exc:  # noqa: BLE001
    print("[FATAL] astropy 不可用 (fail-closed, 判红): %r" % (exc,))
    sys.exit(2)

TOL_MATCH_PX = 1.0e-6
MIN_ORIGIN_SEP_PX = 0.9


def main() -> int:
    binary = os.environ.get("P1PSF_CENTER_GATE_BIN", "")
    if not binary or not os.path.exists(binary):
        print("[FATAL] P1PSF_CENTER_GATE_BIN 缺失或不存在: %r (fail-closed)" % binary)
        return 2
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "center_contract_export.jsonl")
        rc = subprocess.call([binary, "--export", out])
        if rc != 0:
            print("[FATAL] C++ 门 rc=%d (fail-closed)" % rc)
            return 2
        recs, fixture = {}, None
        with open(out, encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                obj = json.loads(line)
                if "fixture" in obj:
                    fixture = obj["fixture"]
                else:
                    recs[obj["key"]] = obj
    if fixture is None:
        print("[FATAL] 门导出缺 fixture (fail-closed)")
        return 2

    w = WCS(naxis=2)
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    w.wcs.crpix = [fixture["crpix1"], fixture["crpix2"]]
    w.wcs.crval = [fixture["crval1"], fixture["crval2"]]
    w.wcs.cd = [[fixture["cd11"], fixture["cd12"]],
                [fixture["cd21"], fixture["cd22"]]]
    # psf 块第 0 点 = index-is-center (0-based) = (10, 12)
    pt0 = (10.0, 12.0)
    print("[astropy %s] CRPIX 1-based (%.1f,%.1f); psf 块 pt0 0-based=(%.1f,%.1f)"
          % (astropy.__version__, fixture["crpix1"], fixture["crpix2"], pt0[0], pt0[1]))

    failures = 0
    px_ref = w.all_world2pix([[recs["photometry_pixelToSky_pt0"]["ra"],
                               recs["photometry_pixelToSky_pt0"]["dec"]]], 0)[0]
    checks = [
        ("photometry_pixelToSky_pt0", "match"),
        ("snr_control_point_pt0", "match"),
        ("wcs_tan_bridged_pt0", "match"),
    ]
    for key, kind in checks:
        rec = recs.get(key)
        if rec is None:
            print("  [FAIL] %-28s 缺失 (fail-closed)" % key)
            failures += 1
            continue
        px = w.all_world2pix([[rec["ra"], rec["dec"]]], 0)[0]
        dpix = math.hypot(px[0] - pt0[0], px[1] - pt0[1])
        ok = dpix <= TOL_MATCH_PX
        failures += 0 if ok else 1
        print("  [%s] %-28s dpx=%.9f (need <= %.1e px vs 0-based label)"
              % ("PASS" if ok else "FAIL", key, dpix, TOL_MATCH_PX))
    # 原点鉴别力: 桥接 vs 未桥接 读法在 astropy 像素域上必须相差 >= 0.9 px
    bu = recs.get("wcs_tan_unbridged_pt0")
    br = recs.get("wcs_tan_bridged_pt0")
    if bu is None or br is None:
        print("  [FAIL] 导出缺 wcs_tan_{un,}bridged_pt0 (fail-closed)")
        failures += 1
    else:
        pu = w.all_world2pix([[bu["ra"], bu["dec"]]], 0)[0]
        pr = w.all_world2pix([[br["ra"], br["dec"]]], 0)[0]
        sep = math.hypot(pu[0] - pr[0], pu[1] - pr[1])
        ok = sep >= MIN_ORIGIN_SEP_PX
        failures += 0 if ok else 1
        print("  [%s] %-28s sep=%.6f px (need >= %.1f px: 未桥接=恒定 1px/轴)"
              % ("PASS" if ok else "FAIL", "origin_discrimination", sep,
                 MIN_ORIGIN_SEP_PX))
    print("[astropy] verdict: %s (failures=%d)"
          % ("PASS" if failures == 0 else "FAIL", failures))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
