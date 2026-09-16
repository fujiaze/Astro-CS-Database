#!/usr/bin/env python3
# NON_PRODUCTION_TOOL_ONLY
"""Generate deterministic catalogue-spatial fuzz oracle vectors (single order).

Uniform-in-solid-angle random points + adversarial points near poles, RA wrap,
and base-face seams. Output JSONL consumed by test_healpix_oracle (ASan build).
"""
from __future__ import annotations
import argparse, json, math, random
from pathlib import Path

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--order", type=int, default=7)
    ap.add_argument("--seed", type=int, default=20260809)
    ap.add_argument("--random", type=int, default=100000)
    ap.add_argument("--adversarial", type=int, default=10000)
    a = ap.parse_args()
    import astropy.units as u
    from astropy_healpix import HEALPix
    hp = HEALPix(nside=1 << a.order, order="nested", frame="icrs")
    rng = random.Random(a.seed)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    with a.out.open("w", encoding="utf-8") as f:
        def emit(ra, dec):
            ip = int(hp.lonlat_to_healpix(ra * u.deg, dec * u.deg))
            f.write(json.dumps({"order": a.order, "ra": round(ra, 12),
                                "dec": round(dec, 12), "ipix": ip}) + "\n")
        # 1) uniform-in-solid-angle random
        for _ in range(a.random):
            ra = rng.random() * 360.0
            dec = math.degrees(math.asin(rng.uniform(-1.0, 1.0)))
            emit(ra, dec)
        # 2) adversarial: poles / RA wrap / face-seam jitter anchors
        anchors = [
            (0.0, 89.999999), (90.0, 89.999999), (180.0, 89.999999),
            (270.0, 89.999999), (0.0, -89.999999), (90.0, -89.999999),
            (180.0, -89.999999), (270.0, -89.999999),
            (0.0, 0.0), (360.0, 0.0), (359.999999, 0.0), (0.000001, 0.0),
            (90.0, 0.0), (180.0, 0.0), (270.0, 0.0),
            (0.0, math.degrees(math.asin(2.0/3.0))),
            (0.0, -math.degrees(math.asin(2.0/3.0))),
            (45.0, 0.0), (135.0, 0.0), (225.0, 0.0), (315.0, 0.0),
            (45.0, 45.0), (135.0, 45.0), (225.0, 45.0), (315.0, 45.0),
            (45.0, -45.0), (135.0, -45.0), (225.0, -45.0), (315.0, -45.0),
        ]
        for ra0, dec0 in anchors:
            for _ in range(a.adversarial // len(anchors)):
                ra = ra0 + rng.uniform(-1e-5, 1e-5)
                dec = dec0 + rng.uniform(-1e-5, 1e-5)
                emit(ra % 360.0, max(-90.0, min(90.0, dec)))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
