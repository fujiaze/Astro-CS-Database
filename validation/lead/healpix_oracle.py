#!/usr/bin/env python3
"""Independent cross check of the HEALPix NESTED index.

The check compares the leaf that the production implementation assigns to a
direction with the leaf that the external astropy-healpix library assigns to
the same direction.  The two are independent: the production code implements
the equal area face chart of SPHERICAL_DRIZZLE section 2 from the formulas,
while astropy-healpix uses the HEALPix C++ implementation.

The directions are drawn from a fixed seed, so the sample is reproducible, and
the comparison is a pure identity check on the leaf index.
"""
from __future__ import annotations

import argparse
import json
import math
import random
import subprocess
import sys
from pathlib import Path

try:
    import astropy.units as u
    import astropy_healpix as ah
except ImportError:  # pragma: no cover - the environment without astropy
    ah = None


def draw_directions(count: int, seed: int):
    rng = random.Random(seed)
    directions = []
    while len(directions) < count:
        v = [rng.gauss(0.0, 1.0) for _ in range(3)]
        norm = math.sqrt(sum(x * x for x in v))
        if norm < 0.1:
            continue
        directions.append([x / norm for x in v])
    return directions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path,
                        help="the small C++ dump program that prints the production leaf")
    parser.add_argument("--orders", default="0,1,2,3,5,7")
    parser.add_argument("--count", type=int, default=3000)
    parser.add_argument("--seed", type=int, default=17)
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    if ah is None:
        print("astropy-healpix is not available; the cross check cannot run", file=sys.stderr)
        return 2

    report = {"format": "acsd.healpix_oracle", "major": 1, "minor": 0,
              "seed": args.seed, "count": args.count, "orders": [], "external": "astropy-healpix"}
    for order in [int(x) for x in args.orders.split(",")]:
        directions = draw_directions(args.count, args.seed + order)
        payload = "\n".join("%.17g %.17g %.17g" % tuple(d) for d in directions)
        completed = subprocess.run([str(args.binary), str(order)], input=payload,
                                   capture_output=True, text=True, check=False)
        if completed.returncode != 0:
            print(f"the production dump failed for order {order}: {completed.stderr}", file=sys.stderr)
            return 1
        produced = [int(x) for x in completed.stdout.split()]
        hp = ah.HEALPix(nside=2 ** order, order="nested")
        agree = 0
        mismatches = []
        for direction, leaf in zip(directions, produced):
            lon = (math.atan2(direction[1], direction[0]) % (2 * math.pi)) * u.rad
            lat = math.asin(direction[2]) * u.rad
            reference = int(hp.lonlat_to_healpix(lon, lat))
            if reference == leaf:
                agree += 1
            elif len(mismatches) < 5:
                mismatches.append({"direction": direction, "production": leaf, "external": reference})
        entry = {"order": order, "compared": len(produced), "agree": agree,
                 "mismatch_examples": mismatches}
        report["orders"].append(entry)
        print(f"order {order}: {agree}/{len(produced)} leaves identical to astropy-healpix")

    total_agree = sum(entry["agree"] for entry in report["orders"])
    total = sum(entry["compared"] for entry in report["orders"])
    report["total_agree"] = total_agree
    report["total_compared"] = total
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{total_agree}/{total} leaves identical to the external reference")
    return 0 if total_agree == total else 1


if __name__ == "__main__":
    sys.exit(main())
