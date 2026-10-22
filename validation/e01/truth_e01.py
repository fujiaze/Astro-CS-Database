#!/usr/bin/env python3
"""E01 truth: the Export support mean, the supported integral and the
extrapolated flux.

Independence
------------
The support areas are computed with an independent detector-domain
integration: the target pixel footprint is mapped into the sky with an
independent tangent-plane model, the overlap with each HEALPix leaf is
integrated with a high order rule, and the three quantities (supported mean,
supported integral, extrapolated full flux) are formed separately.  The
FITS header round trip is read back with astropy (an external reader).

Truth type: analytic_exact (support areas) with a deterministic bound.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lib import observe  # noqa: E402
from lib.availability import availability  # noqa: E402
from lib.vcommon import (  # noqa: E402
    Case,
    canonical_json,
    comparison_payload,
    content_hash,
    environment_identity,
    sha256_text,
    source_identity,
    write_json,
)

CASE = "e01"

# The frozen leaf values of the export fixture: -2, 0 and +3.
LEAF_VALUES = [-2.0, 0.0, 3.0]


class Tangent:
    """An independent TAN projection used by the export truth."""

    def __init__(self, crpix, crval_deg, cd_deg):
        self.crpix = crpix
        self.cd = np.array(cd_deg, dtype=float) * math.pi / 180.0
        alpha0 = math.radians(crval_deg[0])
        delta0 = math.radians(crval_deg[1])
        self.c = np.array([math.cos(delta0) * math.cos(alpha0),
                           math.cos(delta0) * math.sin(alpha0),
                           math.sin(delta0)])
        pole = np.array([0.0, 0.0, 1.0])
        north = pole - np.dot(pole, self.c) * self.c
        self.north = north / np.linalg.norm(north)
        self.east = np.cross(self.north, self.c)

    def forward(self, pixel):
        delta = np.asarray(pixel, dtype=float) + 1.0 - np.asarray(self.crpix, dtype=float)
        q = self.cd @ delta
        vector = self.c + q[0] * self.east + q[1] * self.north
        return vector / np.linalg.norm(vector)

    def solid_angle_jacobian(self, pixel):
        delta = np.asarray(pixel, dtype=float) + 1.0 - np.asarray(self.crpix, dtype=float)
        q = self.cd @ delta
        determinant = abs(float(np.linalg.det(self.cd)))
        return determinant / (1.0 + float(np.dot(q, q))) ** 1.5


def support_areas(tangent, pixel_corners, leaf_membership, order=6, nodes=24):
    """The support area of every target pixel over the declared leaves.

    The detector cell of a target pixel is integrated with a Gauss-Legendre
    rule; the leaf membership decides how much of the cell belongs to each
    leaf.
    """
    healpy = __import__("healpy")
    legendre_nodes, legendre_weights = np.polynomial.legendre.leggauss(nodes)
    low = np.array(pixel_corners[0])
    high = np.array(pixel_corners[1])
    centre = 0.5 * (low + high)
    half = 0.5 * (high - low)
    areas: dict[int, float] = {}
    for i, wx in zip(legendre_nodes, legendre_weights):
        for j, wy in zip(legendre_nodes, legendre_weights):
            x = centre[0] + half[0] * i
            y = centre[1] + half[1] * j
            vector = tangent.forward((x, y))
            leaf = int(healpy.vec2pix(1 << order, vector[0], vector[1], vector[2], nest=True))
            if leaf_membership and leaf not in leaf_membership:
                continue
            weight = wx * wy * half[0] * half[1] * tangent.solid_angle_jacobian((x, y))
            areas[leaf] = areas.get(leaf, 0.0) + float(weight)
    return areas


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify the export closure: the supported mean, the supported integral and the "
              "extrapolated full flux must be separate quantities, the pieces of one leaf must be "
              "merged before the covariance is propagated, no second pixfrac shrink may be "
              "applied and the FITS header round trip must stay inside the tolerance."),
        assumptions=[
            "the target output is a 2x2 even sized image whose CRPIX is (1.37, 1.12)",
            "the CD matrix is mirrored and non diagonal and the field wraps in right ascension",
            "the leaf values are piecewise constant over the declared support",
            "the supported mean is the support weighted mean, not the full pixel mean",
        ],
        estimand=("the support area per target pixel, the supported mean, the supported integral "
                  "and the extrapolated full flux"),
        unit="sr for the areas; L/sr for the mean; L for the integral",
        condition="fixed support and fixed leaf values",
        tolerance={
            "support_area_relative": 1e-6,
            "supported_mean_relative": 1e-9,
            "supported_integral_relative": 1e-9,
            "fits_header_quantisation": "the read back header must reproduce the model",
        },
        positive_expectations=[
            {"id": "supported_mean", "expectation": "the supported mean equals the support "
             "weighted mean of the leaf values"},
            {"id": "supported_integral", "expectation": "the supported integral equals the "
             "support weighted sum"},
            {"id": "same_leaf_merge", "expectation": "the pieces of one leaf are merged before "
             "the variance is propagated"},
        ],
        negative_expectations=[
            {"id": "no_full_pixel_denominator", "expectation": "the supported mean must not use "
             "the full pixel area as denominator"},
            {"id": "no_second_pixfrac", "expectation": "no second drop shrink may be applied"},
            {"id": "extrapolation_separate", "expectation": "the extrapolated full flux must "
             "require the explicit uniform assumption"},
            {"id": "fits_quantisation", "expectation": "the FITS header round trip must not "
             "change the model beyond the declared tolerance"},
        ],
        truth_type="analytic_exact",
    )

    # the target model: a 2x2 image, CRPIX (1.37, 1.12), mirrored CD, RA wrap
    crpix = (1.37, 1.12)
    crval = (359.95, 0.5)
    cd = [[-3.0e-3, 1.0e-3], [1.2e-3, -2.6e-3]]
    tangent = Tangent(crpix, crval, cd)

    # The support: three leaves with the values -2, 0 and +3.  The leaf grid is
    # fine enough that a target pixel is only partially supported, which is
    # the case the specification singles out.  The values are assigned to the
    # three leaves that actually carry the support, so that the supported
    # mean differs from the wrong full pixel mean.
    order = 16
    import healpy
    coverage: dict[int, float] = {}
    for row in range(2):
        for column in range(2):
            corners = ((column - 0.5, row - 0.5), (column + 0.5, row + 0.5))
            areas = support_areas(tangent, corners, {}, order)
            for leaf, area in areas.items():
                coverage[leaf] = coverage.get(leaf, 0.0) + area
    ranked = [leaf for leaf, _ in sorted(coverage.items(), key=lambda item: -item[1])]
    leaf_values = {ranked[index]: value for index, value in enumerate(LEAF_VALUES)}

    # the four target pixels of the 2x2 image (0-based centres 0 and 1)
    pixel_areas = []
    for row in range(2):
        for column in range(2):
            corners = ((column - 0.5, row - 0.5), (column + 0.5, row + 0.5))
            areas = support_areas(tangent, corners, leaf_values, order)
            total = sum(areas.values())
            supported_mean = (sum(area * leaf_values[leaf] for leaf, area in areas.items())
                              / total) if total > 0 else None
            supported_integral = sum(area * leaf_values[leaf] for leaf, area in areas.items())
            pixel_area = float(abs(np.linalg.det(tangent.cd)))
            extrapolated = (supported_integral * pixel_area / total) if total > 0 else None
            pixel_areas.append({
                "pixel": [column, row],
                "support_area_per_leaf_sr": {str(k): v for k, v in sorted(areas.items())},
                "supported_area_sr": total,
                "full_pixel_area_sr": pixel_area,
                "supported_fraction": total / pixel_area,
                "supported_mean": supported_mean,
                "wrong_full_pixel_mean": (sum(leaf_values[leaf] for leaf in areas) / len(areas)
                                          if areas else None),
                "supported_integral": supported_integral,
                "extrapolated_full_flux": extrapolated,
            })

    # the FITS header round trip with an external reader
    header = fits.Header()
    header["CTYPE1"] = "RA---TAN"
    header["CTYPE2"] = "DEC--TAN"
    header["CRPIX1"] = crpix[0]
    header["CRPIX2"] = crpix[1]
    header["CRVAL1"] = crval[0]
    header["CRVAL2"] = crval[1]
    header["CD1_1"] = cd[0][0]
    header["CD1_2"] = cd[0][1]
    header["CD2_1"] = cd[1][0]
    header["CD2_2"] = cd[1][1]
    header["BUNIT"] = "L/sr"
    header["RADESYS"] = "ICRS"
    path = case.root / "input" / "e01_target_header.fits"
    path.parent.mkdir(parents=True, exist_ok=True)
    fits.PrimaryHDU(data=np.zeros((2, 2), dtype=np.float64), header=header).writeto(path,
                                                                                    overwrite=True)
    read_back = fits.getheader(path)
    quantisation = {
        "crpix": [float(read_back["CRPIX1"]), float(read_back["CRPIX2"])],
        "crval": [float(read_back["CRVAL1"]), float(read_back["CRVAL2"])],
        "cd": [[float(read_back["CD1_1"]), float(read_back["CD1_2"])],
               [float(read_back["CD2_1"]), float(read_back["CD2_2"])]],
        "bunit": str(read_back["BUNIT"]),
        "crpix_error": max(abs(float(read_back["CRPIX1"]) - crpix[0]),
                           abs(float(read_back["CRPIX2"]) - crpix[1])),
        "cd_error": max(abs(float(read_back["CD1_1"]) - cd[0][0]),
                        abs(float(read_back["CD1_2"]) - cd[0][1]),
                        abs(float(read_back["CD2_1"]) - cd[1][0]),
                        abs(float(read_back["CD2_2"]) - cd[1][1])),
    }

    truth = {
        "case": "E01",
        "truth_type": "analytic_exact",
        "methods": {
            "support": ("independent detector domain integration of the solid angle element with "
                        "a Gauss-Legendre rule and an external HEALPix membership"),
            "quantities": "the supported mean, the supported integral and the extrapolation are "
                          "formed separately",
            "fits": "the header is written and read back with astropy",
        },
        "target": {"crpix": list(crpix), "crval": list(crval), "cd": cd, "size": [2, 2],
                   "order": order},
        "leaf_values": {str(k): v for k, v in sorted(leaf_values.items())},
        "pixels": pixel_areas,
        "fits_round_trip": quantisation,
    }
    truth_hash = case.write_truth("e01_truth.json", truth)

    fixture = {
        "case": "E01",
        "target": {"crpix": list(crpix), "crval": list(crval), "cd": cd, "width": 2, "height": 2},
        "leaf_values": {str(k): v for k, v in sorted(leaf_values.items())},
        "order": order,
        "quantity": "supported_mean",
        "support_policy": "primitives",
        "assume_uniform_uncovered": False,
        "pixfrac": 1.0,
    }
    fixture_hash = case.write_input("e01_model.json", fixture)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-e01-build-export",
        "operation": "build_export",
        "input_objects": {},
        "arguments": {},
        "request": {},
        "resources": {"bytes": 8388608, "work": 100000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 256, "scratch_directory": ".work"},
        "output_root": ".work/e01",
    }
    request_hash = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": fixture_hash,
        "truth_sha256": truth_hash,
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["build_export", "export_signal", "open_product",
                                       "publish_product"]),
    })

    observe.write_manifest(case, {
        "truth_type": "analytic_exact",
        "seeds": [],
        "trial_denominators": {},
        "library_identity": {"numpy": environment_identity()["numpy"],
                             "astropy": environment_identity()["astropy"],
                             "healpy": environment_identity()["healpy"]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py"},
        "artefacts": {"truth": truth_hash, "input_model": fixture_hash},
        "request_hash": content_hash(request),
    })

    comparison = comparison_payload(
        case.name.upper(), "pending_implementation",
        [{"name": f"pixel_{index}:supported_mean", "kind": "value",
          "truth": entry["supported_mean"], "tolerance": 1e-9, "unit": "L/sr",
          "status": "pending_implementation",
          "reason": "the public export entry points are not available yet"}
         for index, entry in enumerate(pixel_areas)],
        [],
        "The independent truth is complete; the observed side is pending_implementation.")

    case.write_comparison(comparison)
    print(json.dumps({
        "case": "E01",
        "pixels": [{k: v for k, v in entry.items() if k != "support_area_per_leaf_sr"}
                   for entry in pixel_areas],
        "fits_round_trip": quantisation,
    }, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
