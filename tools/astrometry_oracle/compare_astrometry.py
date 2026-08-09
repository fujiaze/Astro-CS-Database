#!/usr/bin/env python3
# NON_PRODUCTION_TOOL_ONLY
"""比较 Astrometry.net 求解 WCS 与参考 WCS / AstroCS 像素→天球链。

模式:
  synthetic: --solved <solved.fits> --ref <known_wcs.json>
  real:      --solved <solved.fits> --ref <astrocs_wcs.json>
             [--corners drizzle_lineage.jsonl --pixels trace_selection.tsv --pixfrac 0.8]

输出: 报告 JSON 到 stdout / --out，含 center/scale/orientation/pixel→sky 指标与 PASS。
"""
from __future__ import annotations
import argparse, json, math, re, sys
from pathlib import Path

def ang_sep(ra1, dec1, ra2, dec2):
    r1, d1, r2, d2 = (math.radians(x) for x in (ra1, dec1, ra2, dec2))
    sd = math.sin((d2 - d1) / 2.0)
    sr = math.sin((r2 - r1) / 2.0)
    h = sd * sd + math.cos(d1) * math.cos(d2) * sr * sr
    return 2.0 * math.asin(min(1.0, math.sqrt(h))) * 180.0 / math.pi

def cd_scale_rot(cd):
    scale = math.sqrt(abs(cd[0][0] * cd[1][1] - cd[0][1] * cd[1][0]))  # deg/px
    rot = math.degrees(math.atan2(cd[1][0], cd[1][1]))  # y 轴相对北的旋转
    return scale, rot

def load_solved(path: Path):
    from astropy.io import fits
    from astropy.wcs import WCS
    hdul = fits.open(path)
    w = WCS(hdul[0].header) if hdul[0].header.get("CTYPE1") else WCS(hdul[1].header)
    hdul.close()
    return w

def pixel_sky_grid(w, size=1024, step=64):
    pts, ra, dec = [], [], []
    for y in range(0, size + 1, step):
        for x in range(0, size + 1, step):
            sky = w.pixel_to_world(x, y)
            ra.append(float(sky.ra.deg)); dec.append(float(sky.dec.deg)); pts.append((x, y))
    return pts, ra, dec

def star_residuals(w, corr_path: Path, ref):
    """匹配星残差: 用参考 WCS 把 Tycho 天球坐标投影为 FITS 1-based 像素，
    再经求解 WCS 反投影回天球，比较与 Tycho 坐标的角距。"""
    import numpy as np
    from astropy.io import fits
    with fits.open(corr_path) as h:
        d = h[1].data
        ra = np.asarray(d["index_ra"], dtype=np.float64)
        dec = np.asarray(d["index_dec"], dtype=np.float64)
    if "field_x" in d.columns.names:
        pix = np.column_stack([np.asarray(d["field_x"], dtype=np.float64),
                               np.asarray(d["field_y"], dtype=np.float64)])  # FITS 1-based
    else:
        from astropy.wcs import WCS
        rw = WCS(naxis=2)
        rw.wcs.crpix = ref["crpix"]; rw.wcs.crval = [ref["center_ra_deg"], ref["center_dec_deg"]]
        rw.wcs.cd = ref["cd"]; rw.wcs.ctype = ["RA---TAN", "DEC--TAN"]
        pix = rw.all_world2pix(np.column_stack([ra, dec]), 1)  # FITS 1-based
    res = []
    for i in range(len(ra)):
        sky = w.pixel_to_world(float(pix[i, 0]) - 1.0, float(pix[i, 1]) - 1.0)  # 0-based
        res.append(ang_sep(ra[i], dec[i], float(sky.ra.deg), float(sky.dec.deg)) * 3600.0)
    res.sort()
    return {"star_median_arcsec": res[len(res)//2], "star_p90_arcsec": res[min(len(res)-1, int(0.9*len(res)))],
            "star_max_arcsec": res[-1], "star_n": len(res)}

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--solved", type=Path, required=True)
    ap.add_argument("--ref", type=Path, required=True)
    ap.add_argument("--corners", type=Path)
    ap.add_argument("--pixels", type=Path)
    ap.add_argument("--corr", type=Path)
    ap.add_argument("--pixfrac", type=float, default=0.8)
    ap.add_argument("--size", type=int, default=1024)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--mode", choices=["synthetic", "real"], default="synthetic")
    args = ap.parse_args()

    w = load_solved(args.solved)
    ref = json.loads(args.ref.read_text(encoding="utf-8"))
    report = {"mode": args.mode, "solved": str(args.solved)}

    # 1) center: 同一图像中心像素在两个 WCS 下的天球投影角距。
    #    ref crpix 为 FITS 1-based；astropy pixel_to_world 用 0-based，
    #    因此换算 cpx0 = crpix - 1（求解器 CRPIX 未必在图像中心，不能直接比 CRVAL）。
    cpx = ref.get("crpix", [args.size / 2 + 0.5, args.size / 2 + 0.5])
    cpx0 = [c - 1.0 for c in cpx]
    sky_at_center = w.pixel_to_world(cpx0[0], cpx0[1])
    rc = (ref.get("center_ra_deg"), ref.get("center_dec_deg"))
    report["center_arcsec"] = ang_sep(rc[0], rc[1],
                                      float(sky_at_center.ra.deg),
                                      float(sky_at_center.dec.deg)) * 3600.0

    # 2) scale & orientation
    if ref.get("cd"):
        ref_scale, ref_rot = cd_scale_rot(ref["cd"])
    else:
        ref_scale = ref.get("scale_arcsec", 6.1878) / 3600.0
        ref_rot = ref.get("rotation_deg", 0.0)
    sol_scale, sol_rot = cd_scale_rot(w.wcs.cd)
    report["ref_scale_arcsec"] = ref_scale * 3600.0
    report["solved_scale_arcsec"] = sol_scale * 3600.0
    report["scale_rel_err"] = abs(sol_scale - ref_scale) / ref_scale
    drot = (sol_rot - ref_rot + 180.0) % 360.0 - 180.0
    report["orientation_err_deg"] = abs(drot)

    # 3) pixel→sky
    res = []
    if args.corners and args.pixels:
        px_map = {}
        for line in args.pixels.read_text().splitlines():
            parts = line.split()
            if len(parts) >= 2 and parts[0].lstrip("-").isdigit() and parts[1].lstrip("-").isdigit():
                px_map[(int(parts[0]), int(parts[1]))] = True
        for line in args.corners.read_text().splitlines():
            if '"x"' not in line:
                continue
            d = json.loads(line)
            key = (int(d["x"]), int(d["y"]))
            if key not in px_map:
                continue
            half = 0.5 * args.pixfrac
            cxy = [[key[0] - half, key[1] - half], [key[0] + half, key[1] - half],
                   [key[0] + half, key[1] + half], [key[0] - half, key[1] + half]]
            for i in range(4):
                sky = w.pixel_to_world(cxy[i][0], cxy[i][1])
                res.append(ang_sep(d["corners_ra"][i], d["corners_dec"][i],
                                   float(sky.ra.deg), float(sky.dec.deg)) * 3600.0)
    else:
        pts, ra, dec = pixel_sky_grid(w, args.size)
        rw = None
        try:
            from astropy.wcs import WCS
            import numpy as np
            rw = WCS(naxis=2)
            rw.wcs.crpix = ref["crpix"]; rw.wcs.crval = rc; rw.wcs.cd = ref["cd"]
            rw.wcs.ctype = ["RA---TAN", "DEC--TAN"]
        except Exception:
            rw = None
        if rw is not None:
            for (x, y) in pts:
                sky = rw.pixel_to_world(x, y)
                res.append(ang_sep(float(sky.ra.deg), float(sky.dec.deg), ra.pop(0), dec.pop(0)) * 3600.0)
    if res:
        res.sort()
        report["pixel_sky_median_arcsec"] = res[len(res) // 2]
        report["pixel_sky_p90_arcsec"] = res[min(len(res) - 1, int(0.9 * len(res)))]
        report["pixel_sky_max_arcsec"] = res[-1]
        report["pixel_sky_n"] = len(res)
    else:
        report["pixel_sky_median_arcsec"] = None
    if args.corr:
        report.update(star_residuals(w, args.corr, ref))

    if args.mode == "synthetic":
        report["PASS"] = (report["center_arcsec"] <= 2.0
                          and report["scale_rel_err"] <= 0.01
                          and report["orientation_err_deg"] <= 0.2
                          and report["star_median_arcsec"] <= 2.0
                          and report["star_max_arcsec"] <= 6.0)
    else:
        report["PASS"] = (report["center_arcsec"] <= 5.0
                          and report["scale_rel_err"] <= 0.02
                          and report["orientation_err_deg"] <= 0.5
                          and report["pixel_sky_median_arcsec"] <= 2.0
                          and report["pixel_sky_max_arcsec"] <= 10.0)
    text = json.dumps(report, indent=2)
    if args.out:
        args.out.write_text(text, encoding="utf-8")
    print(text)
    return 0 if report["PASS"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
