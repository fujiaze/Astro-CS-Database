#!/usr/bin/env python3
"""Generate the locked performance inputs of VERIFICATION.

Two configurations are produced:

* P-small  : 256 x 256 native pixels, HEALPix order 18, TAN with pixfrac 0.63,
             a uniform field plus a 1e9 contrast spot, two frames;
* P-large  : 4096 x 4096 native pixels in eight frames, order 18, a field that
             crosses a face seam, with a second order SIP term.

The files are written with CFITSIO through astropy and carry a plain TAN WCS.
The scientific tolerances used with them are B = 0.01 and Cov = 0.001 in the
named model units; they are recorded in the generated manifest so that a later
measurement can be tied to its inputs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def tan_wcs(width: int, height: int, crval=(150.0, 20.0), cdelt=(1.0 / 3600.0, 1.0 / 3600.0),
             sip_order: int = 0) -> WCS:
    wcs = WCS(naxis=2)
    wcs.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    wcs.wcs.crval = list(crval)
    wcs.wcs.crpix = [(width + 1) / 2.0, (height + 1) / 2.0]
    wcs.wcs.cdelt = list(cdelt)
    if sip_order >= 2:
        wcs.wcs.a_order = sip_order
        wcs.wcs.b_order = sip_order
        wcs.wcs.a = {0: {2: 1.0e-5}, 2: {0: -2.0e-5}}
        wcs.wcs.b = {0: {2: -1.0e-5}, 2: {0: 1.5e-5}}
    return wcs


def write_frame(path: Path, data: np.ndarray, wcs: WCS, exposure: float) -> None:
    header = wcs.to_header()
    header["EXPTIME"] = (exposure, "exposure time in seconds")
    header["BUNIT"] = ("ADU", "raw detector units")
    primary = fits.PrimaryHDU(data.astype(np.float64), header=header)
    primary.writeto(path, overwrite=True)


def make_small(directory: Path) -> dict:
    directory.mkdir(parents=True, exist_ok=True)
    width = height = 256
    rng = np.random.default_rng(20251022)
    background = 200.0
    yy, xx = np.mgrid[0:height, 0:width]
    frames = []
    for index in range(2):
        wcs = tan_wcs(width, height, crval=(150.0 + index * 0.02, 20.0))
        image = background + rng.normal(0.0, 3.0, size=(height, width))
        # A bright source with a contrast of 1e9 over the background.
        image += 1.0e9 * np.exp(-(((xx - 100.5) ** 2 + (yy - 120.5) ** 2) / (2 * 3.0 ** 2)))
        # A second, ordinary source.
        image += 5.0e4 * np.exp(-(((xx - 180.25) ** 2 + (yy - 60.75) ** 2) / (2 * 2.5 ** 2)))
        path = directory / f"p_small_frame_{index:02d}.fits"
        write_frame(path, image, wcs, 100.0)
        frames.append({"path": path.name, "sha256": sha256(path)})
    return {
        "name": "P-small",
        "width": width,
        "height": height,
        "order": 18,
        "pixfrac": 0.63,
        "frames": frames,
        "tolerances": {"brightness": 0.01, "covariance": 0.001},
        "note": "uniform field with a 1e9 contrast spot; the B tolerance is absolute in L/sr",
    }


def make_large(directory: Path) -> dict:
    directory.mkdir(parents=True, exist_ok=True)
    width = height = 4096
    rng = np.random.default_rng(20251023)
    frames = []
    for index in range(8):
        # The field is placed so that consecutive frames straddle a HEALPix
        # face seam, which is the point of the configuration.
        crval = (150.0 + index * 4.6, 20.0 + index * 1.1)
        wcs = tan_wcs(width, height, crval=crval, sip_order=2)
        image = np.full((height, width), 150.0)
        step = 256
        for y in range(0, height, step):
            for x in range(0, width, step):
                image[y:y + step, x:x + step] += 40.0 * ((x // step + y // step) % 3)
        image += rng.normal(0.0, 1.5, size=(height, width))
        path = directory / f"p_large_frame_{index:02d}.fits"
        write_frame(path, image, wcs, 200.0)
        frames.append({"path": path.name, "sha256": sha256(path)})
    return {
        "name": "P-large",
        "width": width,
        "height": height,
        "order": 18,
        "pixfrac": 0.63,
        "frames": frames,
        "tolerances": {"brightness": 0.01, "covariance": 0.001},
        "note": "eight frames across a face seam with a second order SIP term",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path, help="directory for the inputs")
    parser.add_argument("--skip-large", action="store_true",
                        help="only build the small configuration")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {"format": "acsd.performance_inputs", "major": 1, "minor": 0, "configurations": []}
    manifest["configurations"].append(make_small(args.output / "p_small"))
    if not args.skip_large:
        manifest["configurations"].append(make_large(args.output / "p_large"))
    path = args.output / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {path}")
    for configuration in manifest["configurations"]:
        print(f"  {configuration['name']}: {len(configuration['frames'])} frames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
