#!/usr/bin/env python3
"""Independent comparison of the catalogue epoch propagation with astropy.

The script reads validation/cat/catalogue_check.json, recomputes the position
of every sampled source at the requested epoch with astropy (which uses the
ERFA space motion routines) and reports the largest angular difference.

The script is an external oracle: it never reads the production sources.
"""
import json
import math
import pathlib
import sys

try:
    import astropy
    import numpy as np
    from astropy.coordinates import SkyCoord
    from astropy.time import Time
    import astropy.units as u
except ImportError as error:  # pragma: no cover
    print(f"astropy is not available: {error}")
    sys.exit(2)

ROOT = pathlib.Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "validation" / "cat" / "catalogue_check.json"
REPORT = ROOT / "validation" / "cat" / "epoch_check.json"


def main() -> int:
    with EVIDENCE.open(encoding="utf-8") as handle:
        evidence = json.load(handle)

    samples = evidence["epoch_samples"]
    worst_angle = 0.0
    worst_distance = 0.0
    worst = None
    compared = 0
    for sample in samples:
        epoch = sample["epoch_year"]
        target = sample["target_epoch"]
        parallax = sample["parallax_rad"]
        distance = (1.0 / parallax) * u.au if parallax > 0.0 else None
        start = Time(epoch, format="jyear", scale="tdb")
        end = Time(target, format="jyear", scale="tdb")
        coord = SkyCoord(
            ra=sample["ra_rad"] * u.rad,
            dec=sample["dec_rad"] * u.rad,
            distance=distance,
            pm_ra_cosdec=sample["pmra_cosdec_rad_year"] * u.rad / u.yr,
            pm_dec=sample["pmdec_rad_year"] * u.rad / u.yr,
            radial_velocity=(sample["rv_m_s"] * u.m / u.s)
            if sample["rv_present"]
            else 0.0 * u.m / u.s,
            obstime=start,
            frame="icrs",
        )
        moved = coord.apply_space_motion(end)
        reference = SkyCoord(
            ra=sample["propagated_ra_rad"] * u.rad,
            dec=sample["propagated_dec_rad"] * u.rad,
            frame="icrs",
        )
        angle = float(moved.separation(reference).to(u.rad).value)
        if sample["propagated_parallax_rad"] > 0.0:
            distance_difference = abs(
                float(moved.distance.to_value(u.au)) - (1.0 / sample["propagated_parallax_rad"])
            )
        else:
            distance_difference = 0.0
        if angle > worst_angle:
            worst_angle = angle
            worst = sample["source_id"]
        worst_distance = max(worst_distance, distance_difference)
        compared += 1

    report = {
        "case": "catalogue_epoch_propagation",
        "oracle": "astropy " + astropy.__version__,
        "samples": compared,
        "max_angle_difference_rad": worst_angle,
        "max_angle_difference_arcsec": math.degrees(worst_angle) * 3600.0,
        "max_distance_difference_au": worst_distance,
        "worst_source_id": worst,
        "method": "astropy SkyCoord.apply_space_motion (ERFA space motion)",
    }
    with REPORT.open("w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
        handle.write("\n")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
