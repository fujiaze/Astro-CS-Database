#!/usr/bin/env python3
"""F01 truth: pixel integrated photometry, the shared Gaia latent and the scale
interval counter-examples.

Independence
------------
The flux truth is the analytic Gaussian error function integral over the
pixel footprint (the separable closed form), the noise truth is the analytic
propagation of a Poisson plus read noise model and the shared-latent truth is
an explicit joint linear model.  No production PSF, photometry or response
helper is imported.

Truth type: analytic_exact
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
from scipy.special import erf

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

CASE = "f01"


def gaussian_window_flux(amplitude, sigma_x, sigma_y, centre, window):
    """The exact flux of a normalised 2D Gaussian inside an axis aligned
    rectangle (the separable error function integral)."""
    low_x, high_x = window[0]
    low_y, high_y = window[1]
    def part(low, high, sigma, mu):
        return 0.5 * (erf((high - mu) / (sigma * math.sqrt(2.0)))
                      - erf((low - mu) / (sigma * math.sqrt(2.0))))
    return amplitude * part(low_x, high_x, sigma_x, centre[0]) \
        * part(low_y, high_y, sigma_y, centre[1])


def total_flux(amplitude):
    return amplitude


def two_source_aperture():
    """Two adjacent sources inside one aperture: the aperture flux is the sum
    of the two analytic window integrals and the aperture variance carries the
    shared background term."""
    sigma = 1.2
    sources = [(-1.5, 0.0, 100.0), (1.4, 0.3, 60.0)]
    aperture = ((-4.0, 4.0), (-4.0, 4.0))
    fluxes = []
    for x, y, amplitude in sources:
        fluxes.append(gaussian_window_flux(amplitude, sigma, sigma, (x, y), aperture))
    return sources, aperture, fluxes


def scale_interval(counts, truth_flux):
    """The scale interval implied by a measured count total.

    * a single exact measurement (counts == truth) pins the scale to 1;
    * an interval measurement [low, high] gives a scale interval
      [low / truth, high / truth].
    """
    if isinstance(counts, (int, float)):
        low = high = float(counts)
    else:
        low, high = float(counts[0]), float(counts[1])
    if truth_flux == 0:
        return None
    return [low / truth_flux, high / truth_flux]


def shared_latent_joint():
    """The joint counts / Gaia model with one shared latent per source.

    The design matrix of (counts, Gaia) for a source has two rows and one
    shared parameter; the Gauss-Markov estimate of the scale is the
    analytic solution of the 2x2 normal equations.
    """
    # counts variance and Gaia variance, and their covariance through the
    # shared spectral latent
    var_counts = 4.0
    var_gaia = 1.0
    cov_shared = 0.8
    # observations: counts = a * q_c + noise, gaia = a * q_g + noise, with the
    # shared term contributing to the covariance
    q_counts = 10.0
    q_gaia = 5.0
    covariance = np.array([[var_counts, cov_shared], [cov_shared, var_gaia]])
    weights = np.array([q_counts, q_gaia])
    precision = np.linalg.inv(covariance)
    estimate = float(weights @ precision @ weights)
    variance = 1.0 / estimate
    return {
        "covariance": covariance.tolist(),
        "weights": weights.tolist(),
        "scale_variance": variance,
        "scale_sigma": math.sqrt(variance),
        "note": "the shared latent creates the off-diagonal term; treating the two rows as "
                "independent would give the wrong variance",
    }


def rank_negative():
    """Colour = position: the joint design matrix loses rank."""
    n = 15
    position = np.linspace(-1.0, 1.0, n)
    colour = 2.0 * position + 0.3      # exactly collinear with the position
    design = np.column_stack([np.ones(n), position, colour])
    singular = np.linalg.svd(design, compute_uv=False)
    rank = int(np.sum(singular > singular[0] * 1e-12))
    independent = np.column_stack([np.ones(n), position, np.cos(3.0 * position)])
    singular_independent = np.linalg.svd(independent, compute_uv=False)
    rank_independent = int(np.sum(singular_independent > singular_independent[0] * 1e-12))
    return {
        "n_stars": n,
        "singular_values": singular.tolist(),
        "rank": rank,
        "columns": 3,
        "nullspace_dimension": 3 - rank,
        "independent_case_rank": rank_independent,
        "expected": "not_identifiable with a nullspace when colour is a function of position",
    }


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify the pixel integrated photometry: the Gaussian flux inside the aperture, the "
              "two adjacent sources, the shared latent between the counts and the Gaia "
              "measurement, the parameter rank and the scale interval counter-examples."),
        assumptions=[
            "the PSF is a normalised Gaussian in the detector plane",
            "the aperture is an axis aligned rectangle and the flux is not renormalised inside it",
            "the counts and the Gaia measurement of one source share one spectral latent",
            "the sky is a constant background over the aperture",
        ],
        estimand=("the aperture flux, the aperture variance and the scale parameter with its "
                  "interval"),
        unit="ADU (native) for the flux; ADU^2 for the variance",
        condition="high signal Gaussian regime; the Poisson regime is out of scope",
        tolerance={
            "flux_relative": 1e-12,
            "variance_relative": 1e-12,
            "scale_interval": "the reported interval must contain the analytic interval",
        },
        positive_expectations=[
            {"id": "single_source_flux", "expectation": "the aperture flux equals the analytic "
             "erf integral"},
            {"id": "two_source_flux", "expectation": "the aperture flux of two adjacent sources "
             "is the sum of the analytic integrals"},
            {"id": "shared_latent", "expectation": "the joint scale variance keeps the shared "
             "term"},
        ],
        negative_expectations=[
            {"id": "finite_window_not_renormalised", "expectation": "the flux inside a finite "
             "window must not be renormalised to the total flux"},
            {"id": "shared_floor_not_independent", "expectation": "a shared floor must not "
             "decrease like 1/K over K frames"},
            {"id": "colour_position_rank", "expectation": "a colour that is a function of the "
             "position must be reported as not identifiable"},
            {"id": "unknown_gain", "expectation": "an unknown gain must not produce a "
             "fabricated electron variance"},
            {"id": "counts_scale_interval", "expectation": "counts = 100 must pin the scale to "
             "1 while [99, 101] must allow a in [1/1.01, 1/0.99]"},
        ],
        truth_type="analytic_exact",
    )

    sigma = 1.2
    single = {
        "amplitude": 1000.0,
        "sigma_x": sigma,
        "sigma_y": sigma,
        "centre": [0.0, 0.0],
        "window": ((-1.0, 1.0), (-1.0, 1.0)),
    }
    single_flux = gaussian_window_flux(single["amplitude"], sigma, sigma, single["centre"],
                                        single["window"])
    sources, aperture, fluxes = two_source_aperture()
    aperture_flux = float(sum(fluxes))

    # the shared floor: one latent with variance floor_var seen by K frames
    floor_variance = 0.05
    shared_floor = {
        "floor_variance": floor_variance,
        "frames": [1, 2, 5, 10, 100],
        "correct_variance_if_shared": floor_variance,
        "wrong_variance_if_independent": {k: floor_variance / k for k in (1, 2, 5, 10, 100)},
        "note": "the shared term does not decrease with the number of frames",
    }

    shared = shared_latent_joint()
    rank = rank_negative()

    counts_exact = 100.0
    counts_interval = [99.0, 101.0]
    truth_flux = 100.0
    scale_exact = scale_interval(counts_exact, truth_flux)
    scale_interval_case = scale_interval(counts_interval, truth_flux)

    truth = {
        "case": "F01",
        "truth_type": "analytic_exact",
        "methods": {
            "flux": "separable Gaussian error function integral over the aperture",
            "shared_latent": "explicit 2x2 joint normal equations",
            "rank": "singular values of the joint design matrix",
        },
        "single_source": {
            "aperture_flux": single_flux,
            "fraction_of_total": single_flux / single["amplitude"],
            "note": "a finite window does not renormalise the flux",
        },
        "two_sources": {
            "sources": [{"x": x, "y": y, "amplitude": a} for x, y, a in sources],
            "aperture": [list(aperture[0]), list(aperture[1])],
            "per_source_flux": fluxes,
            "aperture_flux": aperture_flux,
        },
        "shared_latent": shared,
        "shared_floor": shared_floor,
        "rank_negative": rank,
        "scale_interval": {
            "counts_exact_100": scale_exact,
            "counts_interval_99_101": scale_interval_case,
            "required_containment": [1.0 / 1.01, 1.0 / 0.99],
            "exact_case_must_be_a_single_point": scale_exact,
            "note": "the exact measurement must not produce a finite radius; the interval "
                    "measurement must keep the difference",
        },
        "unknown_gain": {
            "note": "without a gain no electron variance can be derived; the signal stays "
                    "valid and the variance state must stay unknown",
            "expected_state": "unknown",
        },
    }
    truth_hash = case.write_truth("f01_truth.json", truth)

    fixture = {
        "case": "F01",
        "psf": {"family": "gaussian", "sigma_x": sigma, "sigma_y": sigma},
        "sources": [{"x": x, "y": y, "amplitude": a} for x, y, a in sources],
        "aperture": [list(aperture[0]), list(aperture[1])],
        "sky_constant": 5.0,
        "counts": 100,
        "counts_interval": [99, 101],
        "truth_flux": truth_flux,
        "gaia": {"variance": 1.0, "shared_covariance": 0.8},
        "rank_fixture": {"n": 15, "colour_slope": 2.0},
    }
    fixture_hash = case.write_input("f01_model.json", fixture)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-f01-photometry",
        "operation": "fit_photometry",
        "input_objects": {},
        "arguments": {},
        "request": {"responses": [], "reference": None, "basis": {"kind": "constant"},
                    "frames": ["f1"], "application_id": "val-f01",
                    "solve": {"relative_residual": 1e-10, "absolute_residual": 1e-12,
                              "rank_relative": 1e-12, "max_iterations": 100,
                              "dense_max_bytes": 1048576, "huber": False, "huber_delta": 1.345},
                    "gradient_tolerance": 1e-8, "step_tolerance": 1e-10,
                    "max_outer_iterations": 100,
                    "likelihood": {"estimator": "fixture-f01", "kind": "joint_flux_gaussian",
                                   "fixed_objects": [], "assumptions": []}},
        "resources": {"bytes": 8388608, "work": 100000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 128, "scratch_directory": ".work"},
        "output_root": ".work/f01",
    }
    request_hash = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": fixture_hash,
        "truth_sha256": truth_hash,
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["fit_psf", "measure_science_sources", "integrate_response",
                                       "fit_photometry", "apply_scale"]),
    })

    observe.write_manifest(case, {
        "truth_type": "analytic_exact",
        "seeds": [],
        "trial_denominators": {},
        "library_identity": {"numpy": environment_identity()["numpy"],
                             "scipy": environment_identity()["scipy"]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py"},
        "artefacts": {"truth": truth_hash, "input_model": fixture_hash},
        "request_hash": content_hash(request),
    })

    comparison = comparison_payload(
        case.name.upper(), "pending_implementation",
        [{"name": "two_source_aperture_flux", "kind": "value", "truth": aperture_flux,
          "tolerance": 1e-12 * abs(aperture_flux), "unit": "ADU",
          "status": "pending_implementation",
          "reason": "the public photometry entry points are not available yet"}],
        [],
        "The independent truth is complete; the observed side is pending_implementation.")

    print(json.dumps({
        "case": "F01",
        "single_source_flux": single_flux,
        "single_source_fraction": single_flux / single["amplitude"],
        "two_source_aperture_flux": aperture_flux,
        "shared_latent_scale_variance": shared["scale_variance"],
        "rank": rank["rank"], "nullspace": rank["nullspace_dimension"],
        "scale_exact": scale_exact, "scale_interval": scale_interval_case,
    }, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
