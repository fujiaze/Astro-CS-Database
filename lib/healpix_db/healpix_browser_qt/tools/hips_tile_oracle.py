#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
V11 P11-5: 外部 FITS tile pixel-layout oracle

以 CDS Hipsgen 生成的 REF_ORACLE HiPS 为外部标准：
  - 选 N 个 order7 tile（≥12）；
  - 每 tile 选 M 个 (row,col) 点（corner/edge/center/random，≥32）；
  - 读像素值 → 解码 (ra,dec)（ref_oracle 的 float64 编码）；
  - 用 astropy_healpix（独立实现）求该天球位置的 NESTED leaf local；
  - 与 AstroCS `nested_local_to_fits_index`（(511-x)*512+y, x=偶数位）比对。

可区分：x/y swap、x flip、y flip、transpose、Morton 位序错、face 依赖错。

用法:
  py -3.12 lib/healpix_db/healpix_browser_qt/tools/hips_tile_oracle.py \
      <ref_oracle_hips> [n_tiles] [n_points]
"""

from __future__ import annotations

import json
import random
import re
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits
from astropy_healpix import HEALPix
from astropy.coordinates import ICRS
import astropy.units as u

TW = 512
SHIFT = 9
MASK = (1 << 18) - 1


A = 1_000_000.0
B = 707_106.7811865475


def interleave(x, y, bits):
    local = 0
    for b in range(bits):
        local |= ((x >> b) & 1) << (2 * b)
        local |= ((y >> b) & 1) << (2 * b + 1)
    return local


def astrocs_fits_to_local(row, col):
    """AstroCS 约定：fits_index=(511-x)*512+y；x=偶数位,y=奇数位。"""
    x = TW - 1 - row
    y = col
    return interleave(x, y, SHIFT)


def main() -> int:
    root = Path(sys.argv[1])
    n_tiles = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    n_points = int(sys.argv[3]) if len(sys.argv) > 3 else 32
    rng = random.Random(20260812)

    n7 = root / "Norder7"
    tiles = []
    for p in n7.rglob("Npix*.fits"):
        dm = re.search(r"Dir(\d+)", p.parent.name)
        nm = re.search(r"Npix(\d+)", p.name)
        if dm and nm:
            tiles.append((int(dm.group(1)) * 10000 + int(nm.group(1)), p))
    tiles.sort()
    if len(tiles) < n_tiles:
        print(f"not enough tiles: {len(tiles)} < {n_tiles}")
        return 1
    selected = rng.sample(tiles, n_tiles)

    hp = HEALPix(nside=1 << (7 + SHIFT), order="nested", frame=ICRS())

    def expected_local(ra, dec):
        ipix = int(hp.skycoord_to_healpix(ICRS(ra=ra * u.deg, dec=dec * u.deg)))
        return ipix & MASK, ipix >> 18

    # 候选映射（用于判定公式；AstroCS 为 candA）
    def cands(row, col):
        xA, yA = TW - 1 - row, col          # AstroCS
        xB, yB = row, col
        xC, yC = col, row                   # transpose 候选
        return {
            "astrocs_x=511-row,y=col": interleave(xA, yA, SHIFT),
            "x=row,y=col": interleave(xB, yB, SHIFT),
            "x=col,y=row": interleave(xC, yC, SHIFT),
            "x=511-col,y=511-row": interleave(TW - 1 - col, TW - 1 - row, SHIFT),
            "x=col,y=511-row": interleave(col, TW - 1 - row, SHIFT),
            "x=511-col,y=row": interleave(TW - 1 - col, row, SHIFT),
        }

    stats = {"astrocs_match": 0, "other_match": 0, "no_match": 0, "points": 0}
    other_wins = {}
    rows = []
    ipix_all = np.arange(TW * TW, dtype=np.uint64)
    for tile_ipix, path in selected:
        data = fits.getdata(path).astype(np.float64)
        sky = hp.healpix_to_skycoord(
            ((np.uint64(tile_ipix) << np.uint64(18)) | ipix_all).astype(np.int64))
        lin = A * sky.ra.deg + B * sky.dec.deg
        points = []
        pts = [(0, 0), (0, 511), (511, 0), (511, 511), (0, 256), (256, 0),
               (511, 256), (256, 511), (256, 256)]
        while len(points) < n_points:
            if len(points) < len(pts):
                r, c = pts[len(points)]
            else:
                r, c = rng.randrange(TW), rng.randrange(TW)
            if (r, c) not in points:
                points.append((r, c))
        for (r, c) in points:
            v = data[r, c]
            if not np.isfinite(v):
                continue
            k = int(np.argmin(np.abs(lin - v)))
            exp_local = int(ipix_all[k])
            exp_tile = tile_ipix
            cs = cands(r, c)
            got = {k: (k == "astrocs_x=511-row,y=col") for k in cs}
            match = [k for k, v in cs.items() if v == exp_local]
            stats["points"] += 1
            if "astrocs_x=511-row,y=col" in match:
                stats["astrocs_match"] += 1
            elif match:
                stats["other_match"] += 1
                other_wins[match[0]] = other_wins.get(match[0], 0) + 1
            else:
                stats["no_match"] += 1
            rows.append({
                "tile": tile_ipix, "row": r, "col": c,
                "stored_value": float(v),
                "matched_lin_value": float(lin[k]),
                "expected_local": exp_local, "expected_tile": exp_tile,
                "astrocs_local": astrocs_fits_to_local(r, c),
                "match": "astrocs" if "astrocs_x=511-row,y=col" in match
                         else (match[0] if match else "none"),
            })

    result = {
        "ref_hips": str(root),
        "n_tiles": len(selected),
        "n_points": stats["points"],
        "matches": stats,
        "other_wins": other_wins,
        "detail": rows,
        "verdict": "ASTROCS_MAPPING_MATCHES_EXTERNAL"
                   if stats["no_match"] == 0 and stats["other_match"] == 0
                   else "MAPPING_MISMATCH",
    }
    out = Path("run/temp/p2_v11/evidence/hips_tile_oracle.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items()
                      if k in ("n_tiles", "n_points", "matches", "other_wins",
                               "verdict")}, indent=1))
    return 0 if stats["no_match"] == 0 and stats["other_match"] == 0 else 1


def marker_oracle() -> int:
    """V11 终版：基于 REF_VISUAL 标记的平台值像素做外部判决。
    对每个已知 (plateau 值 → 源图 WCS 天球范围)：
      tile 内 |v-plateau|<tol 的像素，在候选映射下的 astropy cell 中心
      必须落在该 plateau 的天球范围内（+margin）。
    可区分 x/y swap、flip、transpose、位序错。
    """
    import numpy as np
    from astropy.wcs import WCS
    src = Path(sys.argv[1] if len(sys.argv) > 1 else
               "run/temp/p2_v11/ref_src/ref_visual.fits")
    hips = Path(sys.argv[2] if len(sys.argv) > 2 else
                "run/temp/p2_v11/ref_visual_hips")
    hdu = fits.open(src)
    sdata = hdu[0].data
    wcs = WCS(hdu[0].header)
    plateaus = [1000.0, 900.0, 850.0, 950.0, 700.0, 750.0, 800.0]
    extents = {}
    for pv in plateaus:
        ys, xs = np.where(np.abs(sdata - pv) < 0.01)
        if len(ys) == 0:
            continue
        ra, dec = wcs.all_pix2world(xs, ys, 0)
        extents[pv] = (float(ra.min()), float(ra.max()),
                       float(dec.min()), float(dec.max()))
    hp = HEALPix(nside=1 << (7 + SHIFT), order="nested", frame=ICRS())
    margin = 0.012  # 度（约 3.7 个 order7 cell）

    def sky_of(row, col, tile, mode):
        if mode == "astrocs":
            x, y = TW - 1 - row, col
        elif mode == "swap":
            x, y = col, TW - 1 - row
        elif mode == "noswap":
            x, y = row, col
        else:  # identity-flip
            x, y = TW - 1 - row, TW - 1 - col
        local = interleave(x, y, SHIFT)
        leaf = (tile << 18) | local
        sky = hp.healpix_to_skycoord(np.array([leaf], dtype=np.int64))
        return float(sky.ra.deg[0]), float(sky.dec.deg[0])

    modes = ["astrocs", "swap", "noswap", "identity-flip"]
    result = {"modes": {}}
    for mode in modes:
        n_ok = 0
        n_total = 0
        detail = []
        for p in hips.joinpath("Norder7").rglob("Npix*.fits"):
            dm = re.search(r"Dir(\d+)", p.parent.name)
            nm = re.search(r"Npix(\d+)", p.name)
            tile = int(dm.group(1)) * 10000 + int(nm.group(1))
            data = fits.getdata(p).astype(np.float64)
            for pv, (ra0, ra1, dec0, dec1) in extents.items():
                ys, xs = np.where(np.abs(data - pv) < 0.01)
                for (r, c) in zip(ys, xs):
                    n_total += 1
                    sra, sdec = sky_of(int(r), int(c), tile, mode)
                    ok = (ra0 - margin <= sra <= ra1 + margin and
                          dec0 - margin <= sdec <= dec1 + margin)
                    if ok:
                        n_ok += 1
                    else:
                        if len(detail) < 8:
                            detail.append({"tile": tile, "row": int(r),
                                           "col": int(c), "plateau": pv,
                                           "sky_ra": round(sra, 5),
                                           "sky_dec": round(sdec, 5),
                                           "expect_ra": [ra0, ra1],
                                           "expect_dec": [dec0, dec1]})
        result["modes"][mode] = {"ok": n_ok, "total": n_total,
                                 "pass": n_ok == n_total and n_total > 0,
                                 "sample_mismatches": detail}
        print(f"mode={mode}: {n_ok}/{n_total} PASS={n_ok==n_total and n_total>0}")
    out = Path("run/temp/p2_v11/evidence/hips_tile_oracle.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2), encoding="utf-8")
    astrocs = result["modes"]["astrocs"]
    others = [m for k, m in result["modes"].items() if k != "astrocs"]
    print("VERDICT:", "ASTROCS_MAPPING_MATCHES_EXTERNAL"
          if astrocs["pass"] and not any(m["pass"] for m in others)
          else "CHECK_MAPPING")
    return 0 if astrocs["pass"] else 1


if __name__ == "__main__":
    sys.exit(marker_oracle())
