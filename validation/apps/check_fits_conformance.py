#!/usr/bin/env python3
"""Checks the FITS files of the published products with an external reader.

The check is independent of the writer: the files are read with astropy (an
external implementation), the header is inspected for the standard rules and
the file is written back by astropy's strict writer.  A file that the strict
writer refuses is not a standard FITS file.
"""
from __future__ import annotations

import io
import json
import sys
import warnings
from pathlib import Path

try:
    from astropy.io import fits
    import numpy as np
except ImportError:  # pragma: no cover
    print("[SKIP] astropy is not installed")
    sys.exit(0)

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "validation/apps/evidence/fits_conformance"


def check(path: Path) -> dict:
    report = {"file": str(path.relative_to(ROOT)), "bytes": path.stat().st_size}
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        with fits.open(path) as data:
            header = data[0].header
            pixels = data[0].data
            report["shape"] = list(pixels.shape)
            report["dtype"] = str(pixels.dtype)
            report["finite"] = int(np.count_nonzero(np.isfinite(pixels)))
            report["crpix"] = [float(header["CRPIX1"]), float(header["CRPIX2"])] if "CRPIX1" in header else None
            report["crval"] = [float(header["CRVAL1"]), float(header["CRVAL2"])] if "CRVAL1" in header else None
            report["bunit"] = str(header["BUNIT"]) if "BUNIT" in header else None
            report["ctype"] = [str(header["CTYPE1"]), str(header["CTYPE2"])] if "CTYPE1" in header else None
        report["warnings"] = [str(item.message) for item in caught]
    # The external write round trip: the strict writer must accept the header.
    try:
        with fits.open(path) as data:
            buffer = io.BytesIO()
            fits.HDUList(data).writeto(buffer, overwrite=True)
            with fits.open(io.BytesIO(buffer.getvalue())) as back:
                report["round_trip_max_difference"] = float(
                    np.nanmax(np.abs(back[0].data - data[0].data))
                )
        report["round_trip"] = "ok"
    except Exception as failure:  # noqa: BLE001
        report["round_trip"] = f"refused: {failure}"
    return report


def main() -> int:
    targets = []
    export = ROOT / "validation/apps/evidence/export/products/export/signal.fits"
    if export.is_file():
        targets.append(export)
    product = ROOT / ".work/product-roundtrip/frame"
    if (product / "signal").is_dir():
        targets.extend(sorted((product / "signal").rglob("*.fits"))[:4])
    if not targets:
        print("[FAIL] no published FITS file was found")
        return 1
    failures = []
    reports = []
    for path in targets:
        report = check(path)
        reports.append(report)
        print(f"[info] {report['file']}: warnings={len(report['warnings'])} "
              f"round trip={report['round_trip']}")
        for warning in report["warnings"]:
            print(f"        warning: {warning}")
        if report["warnings"]:
            failures.append(f"{report['file']} carries {len(report['warnings'])} warnings")
        if report["round_trip"] != "ok":
            failures.append(f"{report['file']} was refused by the strict writer")
        elif report.get("round_trip_max_difference", 1.0) != 0.0:
            failures.append(f"{report['file']} does not survive the round trip bit exactly")
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / "fits_conformance.json").write_text(
        json.dumps({"reports": reports}, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    if failures:
        for failure in failures:
            print(f"[FAIL] {failure}")
        return 1
    print("[PASS] the published FITS files are standard and survive the external round trip")
    return 0


if __name__ == "__main__":
    sys.exit(main())
