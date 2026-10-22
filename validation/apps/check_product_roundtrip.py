#!/usr/bin/env python3
"""Checks the published product with an independent reader.

The FITS file of the product is parsed here without the library: the header
cards and the big endian float64 data unit are read directly, so the writer of
the product is verified by a second implementation.
"""
from __future__ import annotations

import json
import math
import struct
import sys
from pathlib import Path


def read_fits(path: Path):
    raw = path.read_bytes()
    keys: dict[str, str] = {}
    offset = 0
    while True:
        block = raw[offset : offset + 2880]
        for start in range(0, len(block), 80):
            card = block[start : start + 80].decode("ascii", "replace")
            key = card[:8].strip()
            if key == "END":
                offset += 2880
                return keys, raw[offset:]
            if not key or key in ("COMMENT", "HISTORY"):
                continue
            keys[key] = card[10:].split("/")[0].strip()
        offset += 2880


def main() -> int:
    report = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    root = Path(sys.argv[2])
    failures = []
    for key in ("publish_code", "open_code"):
        if report.get(key) != "ok":
            failures.append(f"{key} is {report.get(key)}")
    if report.get("manifest_format") != "acsd.product":
        failures.append("the manifest format is not acsd.product")
    if report.get("manifest_major") != 2:
        failures.append("the manifest major version is not 2")
    if report.get("manifest_complete") is not True:
        failures.append("the manifest is not complete")
    directory = Path(report["published"])
    for name in ("manifest.json", "frame_science.json"):
        if not (directory / name).is_file():
            failures.append(f"the product has no {name}")
    tile = directory / "signal/Norder6/Dir0/Npix0.fits"
    if not tile.is_file():
        failures.append("the product has no signal tile")
    else:
        keys, data = read_fits(tile)
        if keys.get("BITPIX") != "-64":
            failures.append("the tile is not a float64 image")
        if keys.get("NAXIS") != "3":
            failures.append("the tile is not a three dimensional image")
        values = struct.unpack(f">{len(data) // 8}d", data[: (len(data) // 8) * 8])
        finite = sorted({value for value in values if not math.isnan(value)})
        if finite != [-2.0, 0.0, 3.0]:
            failures.append(f"the tile carries {finite} instead of the three leaf values")
        print(f"[info] tile: {keys.get('NAXIS1')}x{keys.get('NAXIS2')}x{keys.get('NAXIS3')} "
              f"CTYPE1={keys.get('CTYPE1')} CRPIX1={keys.get('CRPIX1')} CD1_1={keys.get('CD1_1')}")
        print(f"[info] the finite values of the tile: {finite}")
    if failures:
        for failure in failures:
            print(f"[FAIL] {failure}")
        return 1
    print("[PASS] the product is published, reopened and the signal is read back")
    return 0


if __name__ == "__main__":
    sys.exit(main())
