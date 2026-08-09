#!/usr/bin/env python3
# NON_PRODUCTION_TOOL_ONLY
"""用 solve-field 对真实 T4 crop 的全部检测源（.axy）渲染合成星场。

每个检测源的真实像素位置经 base WCS 反投影为天球坐标，再由注入 WCS 投影到新像素；
保留真实 FLUX（亮星层级与真实场一致），因此模式可解且服从注入 WCS。
FITS 头部不写 WCS，迫使 solve-field 盲解恢复注入 WCS。

用法:
  py -3.12 gen_synthetic_from_axy.py <axy.fits> <base_wcs.fits> <out_fits> <out_known_json>
    [--scale-factor 0.97] [--rotation-deg 13] [--dra-deg 0.05] [--ddec-deg -0.03]
"""
from __future__ import annotations
import argparse, json
from pathlib import Path

import numpy as np

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("axy", type=Path)
    ap.add_argument("base_wcs", type=Path)
    ap.add_argument("out_fits", type=Path)
    ap.add_argument("out_known_json", type=Path)
    ap.add_argument("--scale-factor", type=float, default=0.97)
    ap.add_argument("--rotation-deg", type=float, default=13.0)
    ap.add_argument("--dra-deg", type=float, default=0.05)
    ap.add_argument("--ddec-deg", type=float, default=-0.03)
    ap.add_argument("--size", type=int, default=1024)
    ap.add_argument("--seed", type=int, default=20260809)
    args = ap.parse_args()

    try:
        from astropy.io import fits
        from astropy.wcs import WCS
    except Exception as exc:
        print("ERROR: astropy unavailable:", exc, file=sys.stderr)
        return 3

    with fits.open(args.axy) as h:
        d = h[1].data
        xs = np.asarray(d["X"], dtype=np.float64)
        ys = np.asarray(d["Y"], dtype=np.float64)
        flux = np.asarray(d["FLUX"], dtype=np.float64)
    w0 = WCS(fits.getheader(args.base_wcs))
    # axy 的 X/Y 为 FITS 1-based；astropy pixel_to_world 用 0-based，故 -1
    sky = w0.pixel_to_world(xs - 1.0, ys - 1.0)

    rot = np.deg2rad(args.rotation_deg)
    cd0 = np.asarray(w0.wcs.cd, dtype=np.float64)
    scale = np.sqrt(abs(np.linalg.det(cd0))) * args.scale_factor
    base = cd0 / np.sqrt(abs(np.linalg.det(cd0)))
    R = np.array([[np.cos(rot), -np.sin(rot)], [np.sin(rot), np.cos(rot)]])
    cd = scale * (R @ base)

    w = WCS(naxis=2)
    w.wcs.crpix = [args.size / 2 + 0.5] * 2
    center0 = w0.pixel_to_world(args.size / 2 + 0.5, args.size / 2 + 0.5)
    w.wcs.crval = [float(center0.ra.deg) + args.dra_deg,
                   float(center0.dec.deg) + args.ddec_deg]
    w.wcs.cd = cd
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    pix = w.all_world2pix(np.column_stack([np.asarray(sky.ra.deg), np.asarray(sky.dec.deg)]), 1)
    xs, ys = pix[:, 0], pix[:, 1]
    # 归一化真实流量: 中位源映射到 ~8000 ADU，保持亮星层级
    flux = np.clip(flux / np.median(flux) * 8000.0, 600.0, 60000.0)

    rng = np.random.default_rng(args.seed)
    img = np.full((args.size, args.size), 1400.0, dtype=np.float64)
    gs = 6
    sig2 = 2.0 * 2.0
    used = 0
    for x0, y0, f in zip(xs - 1.0, ys - 1.0, flux):  # FITS 1-based -> numpy 0-based
        xi, yi = int(np.floor(x0)), int(np.floor(y0))
        if xi + gs < 0 or xi - gs >= args.size or yi + gs < 0 or yi - gs >= args.size:
            continue
        xl, xr = max(0, xi - gs), min(args.size, xi + gs + 1)
        yl, yr = max(0, yi - gs), min(args.size, yi + gs + 1)
        if xr <= xl or yr <= yl:
            continue
        fx = np.arange(xl, xr, dtype=np.float64) - x0
        fy = np.arange(yl, yr, dtype=np.float64) - y0
        g = np.exp(-(fx[None, :] * fx[None, :] + fy[:, None] * fy[:, None]) / (2.0 * sig2))
        g /= g.sum()
        img[yl:yr, xl:xr] += f * g
        used += 1
    img += rng.normal(0.0, 30.0, size=img.shape)
    img = np.clip(np.rint(img), 0, 65535).astype(np.uint16)

    hdr = fits.Header()
    hdr["NAXIS1"] = args.size; hdr["NAXIS2"] = args.size; hdr["BITPIX"] = 16
    hdr["OBJCTRA"] = f"{w.wcs.crval[0]:.6f}"; hdr["OBJCTDEC"] = f"{w.wcs.crval[1]:.6f}"
    fits.writeto(args.out_fits, img, hdr, overwrite=True)
    known = {
        "center_ra_deg": float(w.wcs.crval[0]), "center_dec_deg": float(w.wcs.crval[1]),
        "crpix": [args.size / 2 + 0.5, args.size / 2 + 0.5],
        "scale_arcsec": scale * 3600.0, "rotation_deg": args.rotation_deg,
        "cd": cd.tolist(), "n_sources_used": used,
        "base_wcs": str(args.base_wcs), "scale_factor": args.scale_factor,
    }
    args.out_known_json.write_text(json.dumps(known, indent=2), encoding="utf-8")
    print(f"wrote {args.out_fits}: sources={used} center=({w.wcs.crval[0]:.6f},{w.wcs.crval[1]:.6f}) "
          f"scale={scale*3600:.4f}\"/px rot={args.rotation_deg:.2f}deg")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
