"""The X01 truth comparison: the exported product against the frozen truth.

The comparison is an oracle over the product, not over the implementation:
the seven injected point sources of the truth frame define the expected pixel
positions and relative brightness; the exported image is inspected for local
maxima near those positions.  It reports the astrometric RMS, the recovered
fraction and the largest relative signal error, and writes the observed record.
"""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from astropy.io import fits

CASE_ROOT = Path(__file__).resolve().parent
CLI_DIR = CASE_ROOT / "input" / "cli"
TRUTH = CASE_ROOT / "truth" / "x01_truth.json"
EXPORT = CLI_DIR / "products" / "export" / "signal.fits"
OUTPUT = CASE_ROOT / "observed" / "compare.json"


def background(image: np.ndarray) -> float:
    return float(np.nanmedian(image))


def centroid(image: np.ndarray, x: float, y: float, radius: int = 3) -> tuple[float, float, float] | None:
    x0 = max(0, int(round(x)) - radius)
    x1 = min(image.shape[1], int(round(x)) + radius + 1)
    y0 = max(0, int(round(y)) - radius)
    y1 = min(image.shape[0], int(round(y)) + radius + 1)
    patch = image[y0:y1, x0:x1].astype(float)
    if patch.size == 0:
        return None
    base = background(image)  # one global background, never a local median that a bright source can lift
    weight = np.clip(patch - base, 0.0, None)
    total = float(weight.sum())
    if not total > 0.0:
        return None
    grid_y, grid_x = np.mgrid[y0:y1, x0:x1]
    cx = float((weight * grid_x).sum() / total)
    cy = float((weight * grid_y).sum() / total)
    peak = float(np.nanmax(patch) - base)
    return cx, cy, peak


def main() -> int:
    truth = json.loads(TRUTH.read_text(encoding="utf-8"))
    frame = truth["frames"][0]
    sources = frame["source_field"]
    with fits.open(EXPORT) as handle:
        image = np.asarray(handle[0].data, dtype=float)
    base = background(image)
    matched = []
    for source in sources:
        x, y = source["centre_pixel"]
        found = centroid(image, x, y)
        if found is None:
            continue
        cx, cy, peak = found
        dx = cx - x
        dy = cy - y
        distance = float(np.hypot(dx, dy))
        matched.append({"truth_pixel": [x, y], "measured_pixel": [cx, cy],
                        "offset_pixel": [dx, dy], "distance_pixel": distance,
                        "peak": peak, "truth_weight": float(source["weight"])})
    distances = np.array([m["distance_pixel"] for m in matched], dtype=float)
    rms = float(np.sqrt(np.mean(distances * distances))) if distances.size else float("inf")
    recovered = float(len(matched)) / float(len(sources)) if sources else 0.0
    # The relative brightness is unit free: normalize both the truth weights and
    # the measured peaks to their brightest source.
    signal_error = float("inf")
    if matched:
        truth_max = max(m["truth_weight"] for m in matched)
        peak_max = max(m["peak"] for m in matched)
        errors = []
        for m in matched:
            if peak_max <= 0.0:
                continue
            expected = m["truth_weight"] / truth_max
            measured = m["peak"] / peak_max
            if expected > 0.0:
                errors.append(abs(measured - expected) / expected)
        if errors:
            signal_error = float(max(errors))
    astrometry_tolerance = float(json.loads((CASE_ROOT / "case.json").read_text())["tolerance"]["astrometry_rms_pixel"])
    signal_tolerance = float(json.loads((CASE_ROOT / "case.json").read_text())["tolerance"]["signal_relative"])
    result = {
        "astrometry_rms_pixel": rms,
        "astrometry_tolerance_pixel": astrometry_tolerance,
        "blind_solve": bool(distances.size == len(sources) and rms <= astrometry_tolerance),
        "recovered_fraction": recovered,
        "signal_relative_error": signal_error,
        "signal_tolerance": signal_tolerance,
        "source_recovered": bool(signal_error <= signal_tolerance and matched),
        "matched": matched,
        "sources": len(sources),
        "background": base,
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, indent=1) + "\n", encoding="utf-8")
    print(json.dumps({key: result[key] for key in
                      ("astrometry_rms_pixel", "recovered_fraction", "signal_relative_error",
                       "blind_solve", "source_recovered")}, indent=1))
    return 0 if result["blind_solve"] and result["source_recovered"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
