#!/usr/bin/env python3
"""快速检查 FITS header 的 WCS 关键字"""
import sys
from astropy.io import fits

path = sys.argv[1] if len(sys.argv) > 1 else r"f:\Astro dev\Astro CS Normalization Database\engineering_v1.2\evidence\P10-006\calibrated\T3_LUM_calibrated.fits"
print(f"FITS: {path}")
with fits.open(path, mode="readonly") as hdul:
    h = hdul[0].header
    data = hdul[0].data
    print(f"Image shape: {data.shape} dtype={data.dtype}")
    keys = [
        "CTYPE1", "CTYPE2", "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
        "CD1_1", "CD1_2", "CD2_1", "CD2_2",
        "A_ORDER", "B_ORDER", "AP_ORDER", "BP_ORDER",
        "OBJCTRA", "OBJCTDEC", "FOCALLEN", "XPIXSZ",
        "RADESYS", "EQUINOX",
    ]
    for k in keys:
        v = h.get(k, "MISSING")
        print(f"  {k:12s} = {v}")
