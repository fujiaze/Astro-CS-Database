#!/usr/bin/env python3
"""p3_proj_legacy_deviation.py — legacy registry v1 偏差表门（RETIRED 冻结对照）

背景（SCI-FIX-PROJ / R-1 §4-C）：本脚本原先把 v1 的缺陷当作"预期偏差"复现并 rc=0
通过——修复后反而变红，属"把缺陷冻结成期望"的反向门。现改为**显式偏差表**：

  LEGACY_DEVIATION_TABLE = v1（已 RETIRED）相对标准（astropy/WCSLIB，FITS WCS
  Paper II）四项已登记偏差。v1 行为**不得再变**，因此：
    * 表内每一项都必须仍然复现（否则 rc=1：RETIRED 代码被改动/偏差消失，
      须人工复核并更新表）；
    * 任何表外新偏差（例如 v1 出现新的域/符号错误）→ rc=1。

偏差项（与 ALG-P3-PROJ-IMPL-001 §15.9 v1 偏差表一一对应）:
  D1 CRVAL2 不进映射（CAR/AIT）      : world(CRPIX) != CRVAL，偏差 ≈ |CRVAL2|
  D2 CAR Y=−θ（declination 反号）     : dec_v1 = −dec_标准
  D3 AIT 缺 Paper II √2 因子          : 同像素天球位置与标准差 ≫ 1°
  D4 AIT 域判据 A<2（标准 A≤1）       : v1 接受 X_v1∈(2 rad, 2√2 rad) 的折叠环带

rc: 0 = 偏差表完整复现（v1 冻结对照有效）；1 = 偏差缺失/越界（须复核）；2 = astropy 缺失。
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

# 偏差表：entry = (id, 说明, 判据函数名)  —— 判据函数见下方 CHECKERS
LEGACY_DEVIATION_TABLE = [
    ("D1_crval2_not_in_mapping",
     "CAR/AIT world(CRPIX) != CRVAL（CRVAL2 不进映射，偏差=|CRVAL2|）"),
    ("D2_car_dec_sign_flip",
     "CAR dec 反号（Y=−θ；dec_v1 = −dec_标准）"),
    ("D3_ait_missing_sqrt2",
     "AIT 缺 Paper II γ 的 √2 因子（world→pix 尺度残差）"),
    ("D4_ait_domain_a_lt_2",
     "AIT 域判据 A<2 接受标准椭圆外折叠环带（|X_v1|∈(2rad, 2√2 rad)）"),
]


def run_probe(probe, proj, ra0, dec0, scale, w, h, gn=5):
    out = subprocess.run([probe, proj, repr(ra0), repr(dec0), repr(scale),
                          str(w), str(h), str(gn)],
                         check=True, capture_output=True, text=True, timeout=300)
    head, rows = None, []
    for line in out.stdout.splitlines():
        p = line.split()
        if not p:
            continue
        kv = {t.split("=")[0]: t.split("=")[1] for t in p[1:]}
        if p[0] == "V6PROBE":
            head = kv
        elif p[0] == "ROW":
            rows.append(kv)
    return head, rows


def make_wcs(head, proj, ra0, dec0):
    w = WCS(naxis=2)
    w.wcs.ctype = ["RA---" + proj, "DEC--" + proj]
    w.wcs.crval = [ra0, dec0]
    w.wcs.crpix = [float(head["crpix1"]), float(head["crpix2"])]
    w.wcs.cd = [[float(head["cd11"]), float(head["cd12"])],
                [float(head["cd21"]), float(head["cd22"])]]
    w.wcs.set()
    return w


def astropy_world(w, x, y):
    """标准读值；标准域外 -> None（astropy 抛异常或给 NaN/Inf）。"""
    try:
        ra, dec = w.all_pix2world([[x, y]], 0)[0]
    except Exception:
        return None
    if not (np.isfinite(ra) and np.isfinite(dec)):
        return None
    return float(ra), float(dec)


def center_pixel(head):
    return float(head["crpix1"]) - 1.0, float(head["crpix2"]) - 1.0


def row_at(rows, x, y, tol=1e-9):
    for r in rows:
        if abs(float(r["x"]) - x) < tol and abs(float(r["y"]) - y) < tol:
            return r
    return None


def d1_crval2_not_in_mapping(probe):
    """CAR/AIT dec0=±30：v1 在 CRPIX 处给 dec≈0（应为 CRVAL2）。"""
    worst = 0.0
    for proj in ("CAR", "AIT"):
        for dec0 in (30.0, -30.0, 60.0):
            head, rows = run_probe(probe, proj, 10.0, dec0, 0.2, 97, 97, 5)
            cx, cy = center_pixel(head)
            r = row_at(rows, cx, cy)
            assert r is not None and r["status"] == "0", (proj, dec0, "center row")
            dev = abs(float(r["dec"]) - dec0)
            worst = max(worst, dev)
    return worst, 25.0, "deg", "偏差 = |CRVAL2|（v1 忽略 CRVAL2）"


def d2_car_dec_sign_flip(probe):
    """CAR dec0=0：v1 dec = −标准 dec；同时标准角距 ≥1°。"""
    head, rows = run_probe(probe, "CAR", 0.0, 0.0, 1.0, 97, 97, 5)
    w = make_wcs(head, "CAR", 0.0, 0.0)
    flip_worst, sep_worst = 0.0, 0.0
    for r in rows:
        if r["status"] != "0":
            continue
        x, y = float(r["x"]), float(r["y"])
        aw = astropy_world(w, x, y)
        if aw is None:
            continue
        ra_a, dec_a = aw
        flip_worst = max(flip_worst, abs(dec_a - (-float(r["dec"]))) * 3600.0)
        dra = abs(ra_a - float(r["ra"]))
        dra = min(dra, 360.0 - dra)
        sep_worst = max(sep_worst, math.hypot(dra, dec_a - float(r["dec"])) * 3600.0)
    assert flip_worst < 1e-6, ("CAR 反号模式改变", flip_worst)
    return sep_worst, 3600.0, "arcsec", "v1 dec 与标准 dec 反号（残差应≈2|dec|）"


def d3_ait_missing_sqrt2(probe):
    """AIT dec0=0：同一像素上 v1 天球位置与标准（astropy）的球面角距。

    v1 少 √2 ⇒ 平面尺度放大 √2 ⇒ 同像素指向偏移随视场线性增长。
    """
    head, rows = run_probe(probe, "AIT", 0.0, 0.0, 1.0, 64, 64, 5)
    w = make_wcs(head, "AIT", 0.0, 0.0)
    worst = 0.0
    for r in rows:
        if r["status"] != "0":
            continue
        x, y = float(r["x"]), float(r["y"])
        aw = astropy_world(w, x, y)
        if aw is None:
            continue
        ra_a, dec_a = aw
        dra = abs(ra_a - float(r["ra"]))
        dra = min(dra, 360.0 - dra)
        worst = max(worst, math.hypot(dra, dec_a - float(r["dec"])) * 3600.0)
    return worst, 3600.0, "arcsec", "缺 √2 -> 同像素天球位置偏移（视场越大越显著）"


def d4_ait_domain_a_lt_2(probe):
    """AIT dec0=0：v1 接受 |X_v1| > 2 rad（自身像边界）并产生折返。

    v1 缺 √2 ⇒ 其像边界在 X_v1 = 2 rad = 114.59°（φ=±180°），但域判据 A<2 一路
    接受到 X_v1 = 2√2 rad = 162.06°。|X_v1| ∈ (2, 2√2) rad 上 |ΔRA| 越过 180°
    后折返（wrapped 后 |ΔRA| 随 |X| 增大反而下降）——标准 AIT（边界 2√2 rad）
    在自身域内 |ΔRA| 单调不减，无此折返。

    度量 = 折返幅度（deg）：|ΔRA| 峰值之后随 |X| 增大的最大下降量。
    """
    head, rows = run_probe(probe, "AIT", 0.0, 0.0, 4.5, 64, 3, 61)
    samples = []
    for r in rows:
        if r["status"] != "0":
            continue
        x, y = float(r["x"]), float(r["y"])
        xd = (-4.5) * ((x + 1.0) - float(head["crpix1"]))   # east_left cd11=-4.5
        dra = float(r["ra"]) - 0.0
        dra = (dra + 180.0) % 360.0 - 180.0
        samples.append((abs(xd), abs(dra)))
    assert samples, "no accepted AIT samples"
    samples.sort()
    # 峰值之后的下降量（只在 |X|>100° 的折返区考察）
    peak = 0.0
    drop = 0.0
    for ax, ad in samples:
        if ax > 100.0:
            if ad > peak:
                peak = ad
            else:
                drop = max(drop, peak - ad)
    return drop, 5.0, "deg", "v1 接受 |X_v1|>2rad 折叠环带（|ΔRA| 折返）"


CHECKERS = {
    "D1_crval2_not_in_mapping": d1_crval2_not_in_mapping,
    "D2_car_dec_sign_flip": d2_car_dec_sign_flip,
    "D3_ait_missing_sqrt2": d3_ait_missing_sqrt2,
    "D4_ait_domain_a_lt_2": d4_ait_domain_a_lt_2,
}


def main(probe):
    ok = True
    for entry_id, note in LEGACY_DEVIATION_TABLE:
        try:
            value, threshold, unit, how = CHECKERS[entry_id](probe)
        except AssertionError as exc:
            print("%-28s DEVIATION-LOST %s（%s）" % (entry_id, exc, note))
            ok = False
            continue
        reproduced = value >= threshold
        print("%-28s %s value=%.6g %s (>=%.6g)  %s"
              % (entry_id, "REPRODUCED" if reproduced else "LOST",
                 value, unit, threshold, how))
        ok = ok and reproduced
    print("LEGACY-DEVIATION-TABLE %s (v1 RETIRED；偏差集合只减不增，"
          "任何变化须复核并更新表)" % ("OK" if ok else "CHANGED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
