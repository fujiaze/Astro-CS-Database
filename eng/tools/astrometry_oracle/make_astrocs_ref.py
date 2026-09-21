#!/usr/bin/env python3
# NON_PRODUCTION_TOOL_ONLY
"""从 AstroCS drizzle lineage（像素四角 RA/Dec）推导参考 WCS 参数。

输出 astrocs_wcs.json: center(plate solve CRVAL) / scale(角间距中位数) /
rotation(中心像素局部轴方向) / pixfrac。
"""
from __future__ import annotations
import argparse, json, math
from pathlib import Path

def sep(ra1, dec1, ra2, dec2):
    r1, d1, r2, d2 = (math.radians(x) for x in (ra1, dec1, ra2, dec2))
    h = math.sin((d2-d1)/2)**2 + math.cos(d1)*math.cos(d2)*math.sin((r2-r1)/2)**2
    return 2.0*math.asin(min(1.0, math.sqrt(h)))

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("drizzle_lineage", type=Path)
    ap.add_argument("out_json", type=Path)
    ap.add_argument("--center-ra", type=float, default=272.886341)
    ap.add_argument("--center-dec", type=float, default=-23.254003)
    ap.add_argument("--crpix", type=float, nargs=2, default=[512.5, 512.5])
    ap.add_argument("--pixfrac", type=float, default=0.8)
    args = ap.parse_args()

    scales, rots, n = [], [], 0
    center_rot = None
    for line in args.drizzle_lineage.read_text().splitlines():
        if '"corners_ra"' not in line:
            continue
        d = json.loads(line)
        ra, dec = d["corners_ra"], d["corners_dec"]
        sx = sep(ra[0], dec[0], ra[1], dec[1]) * 180.0 / math.pi * 3600.0 / args.pixfrac
        sy = sep(ra[0], dec[0], ra[3], dec[3]) * 180.0 / math.pi * 3600.0 / args.pixfrac
        scales.append(0.5 * (sx + sy))
        # x 轴（corner0->corner1）相对 RA 增量的方位角（像素 y 向下）
        dx = ra[1] - ra[0]
        if dx > 180.0: dx -= 360.0
        if dx < -180.0: dx += 360.0
        rot = math.degrees(math.atan2(dec[1] - dec[0], dx * math.cos(math.radians(dec[0]))))
        rots.append(rot)
        if d["x"] == 512 and d["y"] == 512:
            center_rot = rot
        n += 1
    if n == 0:
        print("ERROR: no drizzle lineage rows", file=sys.stderr)
        return 2
    scales.sort(); rots.sort()
    ref = {
        "center_ra_deg": args.center_ra, "center_dec_deg": args.center_dec,
        "crpix": list(args.crpix), "scale_arcsec": scales[len(scales)//2],
        "rotation_deg": center_rot if center_rot is not None else rots[len(rots)//2],
        "pixfrac": args.pixfrac, "n_pixels": n,
        "scale_median_arcsec": scales[len(scales)//2], "scale_p10_arcsec": scales[int(0.1*n)],
        "scale_p90_arcsec": scales[min(n-1, int(0.9*n))],
    }
    args.out_json.write_text(json.dumps(ref, indent=2), encoding="utf-8")
    print(json.dumps(ref, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
