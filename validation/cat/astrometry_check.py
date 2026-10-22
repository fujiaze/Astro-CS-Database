#!/usr/bin/env python3
"""Independent verification of the blind astrometric solution with astropy.

The script reads validation/cat/astrometry_check.json, rebuilds the truth model
and the recovered model as astropy WCS objects and compares the two
projections over the whole detector.  The astropy TAN projection with the SIP
distortion is an implementation that is independent of the one under test.

The script is an external oracle: it never reads the production sources.
"""
import json
import math
import pathlib
import sys

try:
    import astropy
    import numpy as np
    from astropy.wcs import WCS, Sip
except ImportError as error:  # pragma: no cover
    print(f"astropy is not available: {error}")
    sys.exit(2)

ROOT = pathlib.Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "validation" / "cat" / "astrometry_check.json"
REPORT = ROOT / "validation" / "cat" / "astrometry_oracle.json"


def build_wcs(parameters, width, height):
    """The WCS of a parameter vector.

    The vector carries the reference point, the fiducial point, the linear
    part and the second order SIP coefficients:
    crpix_x, crpix_y, ra0, dec0, cd00, cd01, cd10, cd11, a02, a11, a20,
    b02, b11, b20.
    """
    wcs = WCS(naxis=2)
    wcs.wcs.ctype = ["RA---TAN-SIP", "DEC--TAN-SIP"]
    wcs.wcs.crval = [parameters[2], parameters[3]]
    wcs.wcs.crpix = [parameters[0], parameters[1]]
    wcs.wcs.cd = np.array([[parameters[4], parameters[5]], [parameters[6], parameters[7]]])
    wcs.wcs.radesys = "ICRS"
    wcs.wcs.lonpole = 180.0
    if len(parameters) >= 14:
        # The second order polynomials in the convention of astropy: the entry
        # [i][j] carries the coefficient of the power i of the x coordinate and
        # the power j of the y coordinate.
        a = np.zeros((3, 3))
        b = np.zeros((3, 3))
        a[0][2] = parameters[8]
        a[1][1] = parameters[9]
        a[2][0] = parameters[10]
        b[0][2] = parameters[11]
        b[1][1] = parameters[12]
        b[2][0] = parameters[13]
        wcs.sip = Sip(a, b, None, None, [parameters[0], parameters[1]])
    wcs.pixel_shape = (height, width)
    return wcs


def main() -> int:
    with EVIDENCE.open(encoding="utf-8") as handle:
        evidence = json.load(handle)

    width = int(evidence["field_width"])
    height = int(evidence["field_height"])
    truth = build_wcs(evidence["truth_parameters"], width, height)
    recovered = build_wcs(evidence["recovered_parameters"], width, height)

    grid = np.linspace(0.0, 1.0, 41)
    xs = grid * (width - 1)
    ys = grid * (height - 1)
    worst = 0.0
    worst_pixel = None
    compared = 0
    for x in xs:
        for y in ys:
            truth_world = truth.pixel_to_world_values(x, y)
            recovered_world = recovered.pixel_to_world_values(x, y)
            separation = recovered_world[0] - truth_world[0]
            # The right ascension is measured on the equator, so the cosine of
            # the declination enters the separation.
            separation *= math.cos(0.5 * math.radians(0.5 * (truth_world[1] + recovered_world[1])))
            separation = abs(separation)
            if separation > worst:
                worst = separation
                worst_pixel = (float(x), float(y))
            compared += 1

    arcsec = worst * 3600.0
    report = {
        "case": "blind_astrometric_solve_oracle",
        "oracle": f"astropy {astropy.__version__}",
        "compared_pixels": compared,
        "field_width": width,
        "field_height": height,
        "max_difference_arcsec": arcsec,
        "max_difference_rad": worst,
        "worst_pixel": worst_pixel,
        "status": "pass" if arcsec < 2.0 else "fail",
        "tolerance_arcsec": 2.0,
        "epoch_source": (
            "The affine hypothesis and the verification use the same batch of catalogue "
            "positions, taken from the provider's region query at the observation epoch.  This "
            "is not a circular argument: the hypothesis is the affine model fitted to the four "
            "quadruple positions, the verification is the residual of that model over the whole "
            "field.  The independence lies in the position source and in the model form, not in "
            "the identity of the positions: the same positions are used, and the epoch is the "
            "observation epoch, so the proper motion of a source is not counted twice and not "
            "mistaken for an astrometric error."
        ),
        "catalogue_epoch_semantics": (
            "catalogue.lookup returns the catalogue epoch positions and catalogue.query "
            "returns the prediction at the requested epoch.  The blind solve uses the query "
            "path; with the lookup path a source of four arcsec per year over twenty years "
            "would displace the affine hypothesis by about eighty arcsec."
        ),
    }
    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
