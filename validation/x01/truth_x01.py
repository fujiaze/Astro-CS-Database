#!/usr/bin/env python3
"""X01 truth: the end-to-end synthetic observation.

Independence
------------
The raw ADU frames are built here from a physical spectral radiance
I_lambda(omega, t): the photon rate of every pixel is an independent
integration of the source SED, the response and the pixel integrated PSF, and
the frames are drawn with an explicit Poisson plus read noise model.  The
data are written as FITS files that the implementation under test must
consume through the three commands; no production reader is used to create
them and the truth never reads an observed product.

Truth type: empirical (a synthetic observation with a known construction)
backed by analytic_exact for the pixel integrated PSF.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits
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
    sha256_file,
    sha256_text,
    source_identity,
    write_json,
)

CASE = "x01"
SIZE = 128
FRAMES = 8
SKY_FRAMES = 3
SEED = 20261029

# physical constants
PLANCK = 6.62607015e-34
SPEED_OF_LIGHT = 2.99792458e8


def continuum_sed(wavelength_nm, amplitude, pivot_nm=550.0):
    """A smooth continuum in W m^-2 nm^-1."""
    return amplitude * (wavelength_nm / pivot_nm) ** -1.5


def line_sed(wavelength_nm, centre_nm, width_nm, amplitude):
    """A narrow Gaussian line."""
    return amplitude * np.exp(-0.5 * ((wavelength_nm - centre_nm) / width_nm) ** 2)


def photon_integral(wavelength_nm, flux_w_m2_nm, throughput):
    """The photon rate per unit area: int f R lambda/(hc) dlambda [s^-1 m^-2]."""
    lam = wavelength_nm * 1e-9
    integrand = flux_w_m2_nm * throughput * lam / (PLANCK * SPEED_OF_LIGHT)
    return float(np.trapezoid(integrand, wavelength_nm))


def pixel_integrated_gaussian(sigma, centre, pixel):
    """The exact integral of a normalised Gaussian over one pixel."""
    low = pixel - 0.5
    high = pixel + 0.5
    return 0.5 * (erf((high - centre) / (sigma * math.sqrt(2.0)))
                  - erf((low - centre) / (sigma * math.sqrt(2.0))))


def build_frame(seed, psf_sigma, throughput, gain, read_rms, sky_level, dark_level,
                exposure_seconds, with_source=True):
    """One raw ADU frame with a known construction."""
    rng = np.random.default_rng(seed)
    electrons = np.zeros((SIZE, SIZE), dtype=float)

    if with_source:
        wavelength = np.linspace(400.0, 700.0, 3001)
        sed = continuum_sed(wavelength, 1.0e-18)
        sed = sed + line_sed(wavelength, 656.3, 1.5, 6.7e-18)
        rate = photon_integral(wavelength, sed, throughput)
        centre = (63.5, 64.5)
        for row in range(SIZE):
            for column in range(SIZE):
                kernel_x = pixel_integrated_gaussian(psf_sigma, centre[0], column)
                kernel_y = pixel_integrated_gaussian(psf_sigma, centre[1], row)
                electrons[row, column] = rate * kernel_x * kernel_y

    # the sky and the dark are additive electron rates
    electrons += sky_level
    electrons += dark_level
    # the expectation is an electron rate; the exposure converts it to counts
    electrons *= exposure_seconds

    photons = rng.poisson(electrons)
    read = rng.normal(0.0, read_rms, size=electrons.shape)
    adu = (photons + read) / gain + 0.0
    return adu, electrons


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Run the full chain on a synthetic observation whose construction is known: the raw "
              "ADU frames must be reduced to a per frame signal HiPS, mosaicked and exported, "
              "and the group without a WCS must actually be blind solved."),
        assumptions=[
            "the detector is linear and unsaturated",
            "the PSF is a normalised Gaussian in the detector plane",
            "the noise is Poisson photon noise plus an independent Gaussian read noise",
            "one of the two response groups has no WCS header and must be blind solved",
        ],
        estimand=("the exported signal, the per frame calibration and the blind astrometric "
                  "solution of the group without a WCS"),
        unit="ADU in the raw frames; L/sr in the products",
        condition="the synthetic observation described by the input files",
        tolerance={"signal_relative": 0.05,
                   "astrometry_rms_pixel": 0.5,
                   "blind_solve": "the group without a WCS must produce a solution"},
        positive_expectations=[
            {"id": "three_commands", "expectation": "normalize, mosaic and export all run on "
             "the disk product"},
            {"id": "blind_solve", "expectation": "the group without a WCS is blind solved and "
             "the residual is reported"},
            {"id": "source_recovered", "expectation": "the point source is recovered near its "
             "injected position"},
        ],
        negative_expectations=[
            {"id": "gui_is_only_smoke", "expectation": "the GUI is only a protocol smoke test "
             "and never a substitute for the science path"},
            {"id": "no_fabricated_solution", "expectation": "a failed blind solve must report "
             "unsolved and publish nothing"},
        ],
        truth_type="empirical + analytic_exact",
    )

    frames_dir = case.root / "input" / "frames"
    frames_dir.mkdir(parents=True, exist_ok=True)

    wavelength = np.linspace(400.0, 700.0, 3001)
    throughput_a = 0.6 * np.exp(-0.5 * ((wavelength - 520.0) / 60.0) ** 2)
    throughput_b = 0.45 * np.exp(-0.5 * ((wavelength - 610.0) / 45.0) ** 2)

    frame_files = []
    frame_truth = []
    for index in range(FRAMES):
        group = "A" if index < FRAMES // 2 else "B"
        throughput = throughput_a if group == "A" else throughput_b
        sky_level = 40.0 + 5.0 * index
        adu, electrons = build_frame(
            seed=SEED + index, psf_sigma=1.35, throughput=throughput, gain=2.5,
            read_rms=3.0, sky_level=sky_level, dark_level=12.0,
            exposure_seconds=100.0, with_source=True)
        path = frames_dir / f"frame_{index:02d}_{group}.fits"
        header = fits.Header()
        header["FRAMEID"] = f"x01-{index:02d}"
        header["GROUP"] = group
        header["EXPTIME"] = 100.0
        header["GAIN"] = 2.5
        header["READNOISE"] = 3.0
        header["BUNIT"] = "ADU"
        if group == "A":
            # the A group carries a WCS, the B group does not and must be
            # blind solved
            header["CTYPE1"] = "RA---TAN"
            header["CTYPE2"] = "DEC--TAN"
            header["CRPIX1"] = 64.0
            header["CRPIX2"] = 64.0
            header["CRVAL1"] = 83.82208
            header["CRVAL2"] = -5.39111
            header["CD1_1"] = -0.0002777778
            header["CD1_2"] = 0.0
            header["CD2_1"] = 0.0
            header["CD2_2"] = 0.0002777778
            header["RADESYS"] = "ICRS"
        fits.PrimaryHDU(data=adu, header=header).writeto(path, overwrite=True)
        frame_files.append({"path": str(path.relative_to(case.root.parent)),
                            "sha256": sha256_file(path), "group": group,
                            "sky_level_electrons": sky_level})
        frame_truth.append({
            "frame": index, "group": group,
            "total_electrons": float(electrons.sum()),
            "sky_level_electrons": sky_level,
            "dark_level_electrons": 12.0,
            "gain_e_per_adu": 2.5, "read_rms_e": 3.0,
            "psf_sigma_pixel": 1.35,
            "source_centre_pixel": [63.5, 64.5] if group == "A" else None,
        })

    sky_files = []
    for index in range(SKY_FRAMES):
        rng = np.random.default_rng(SEED + 500 + index)
        sky_electrons = 55.0 + 3.0 * index
        adu = (rng.poisson(np.full((SIZE, SIZE), sky_electrons * 100.0)) +
               rng.normal(0.0, 3.0, size=(SIZE, SIZE))) / 2.5
        path = frames_dir / f"sky_{index:02d}.fits"
        header = fits.Header()
        header["FRAMEID"] = f"x01-sky-{index:02d}"
        header["EXPTIME"] = 100.0
        header["BUNIT"] = "ADU"
        fits.PrimaryHDU(data=adu, header=header).writeto(path, overwrite=True)
        sky_files.append({"path": str(path.relative_to(case.root.parent)),
                          "sha256": sha256_file(path),
                          "sky_level_electrons": sky_electrons})

    sed_truth = {
        "wavelength_nm_min": 400.0,
        "wavelength_nm_max": 700.0,
        "continuum_amplitude_w_m2_nm": 1.0e-18,
        "line_centre_nm": 656.3,
        "line_width_nm": 1.5,
        "line_amplitude_w_m2_nm": 6.7e-18,
        "photon_rate_group_a_s_m2": photon_integral(
            wavelength,
            continuum_sed(wavelength, 1.0e-18) + line_sed(wavelength, 656.3, 1.5, 6.7e-18),
            throughput_a),
        "photon_rate_group_b_s_m2": photon_integral(
            wavelength,
            continuum_sed(wavelength, 1.0e-18) + line_sed(wavelength, 656.3, 1.5, 6.7e-18),
            throughput_b),
    }

    truth = {
        "case": "X01",
        "truth_types": ["empirical", "analytic_exact"],
        "methods": {
            "photon_rate": "int f R lambda/(hc) dlambda with a trapezoid rule on the sampled SED",
            "pixel_integration": "the exact separable error function integral of a Gaussian",
            "noise": "independent Poisson photons plus an independent Gaussian read noise",
        },
        "frames": frame_truth,
        "sky_frames": sky_files,
        "spectral_model": sed_truth,
        "groups": {
            "A": {"wcs": True, "frames": [0, 1, 2, 3], "throughput": "group A"},
            "B": {"wcs": False, "frames": [4, 5, 6, 7],
                  "throughput": "group B",
                  "note": "the B group carries no WCS and must be blind solved"},
        },
        "seed": SEED,
    }
    truth_hash = case.write_truth("x01_truth.json", truth)

    fixture = {
        "case": "X01",
        "frames": frame_files,
        "sky_frames": sky_files,
        "size": [SIZE, SIZE],
        "groups": truth["groups"],
        "spectral_model": sed_truth,
        "seed": SEED,
    }
    fixture_hash = case.write_input("x01_model.json", fixture)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-x01-normalize",
        "operation": "normalize",
        "input_objects": {},
        "arguments": {},
        "request": {},
        "resources": {"bytes": 268435456, "work": 10000000, "spill_bytes": 8589934592,
                      "threads": 4, "max_depth": 64, "max_precision_bits": 256,
                      "scratch_directory": ".work/x01"},
        "output_root": ".work/x01",
    }
    request_hash = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": fixture_hash,
        "truth_sha256": truth_hash,
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["normalize", "mosaic", "export_image",
                                       "solve_astrometry"]),
    })

    observe.write_manifest(case, {
        "truth_type": "empirical + analytic_exact",
        "seeds": {"synthetic_frames": SEED},
        "trial_denominators": {"frames": FRAMES, "sky_frames": SKY_FRAMES},
        "library_identity": {"numpy": environment_identity()["numpy"],
                             "astropy": environment_identity()["astropy"],
                             "scipy": environment_identity()["scipy"]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py"},
        "artefacts": {"truth": truth_hash, "input_model": fixture_hash},
        "request_hash": content_hash(request),
    })

    comparison = comparison_payload(
        case.name.upper(), "pending_implementation",
        [{"name": "three_commands_on_disk", "kind": "value", "truth": "ok", "tolerance": 0.0,
          "unit": "verdict", "status": "pending_implementation",
          "reason": "the three command entry points are not available yet"}],
        [],
        "The synthetic observation is complete and frozen; the observed side is "
        "pending_implementation.")

    print(json.dumps({"case": "X01",
                      "frames": len(frame_files), "sky_frames": len(sky_files),
                      "photon_rate_a": sed_truth["photon_rate_group_a_s_m2"],
                      "photon_rate_b": sed_truth["photon_rate_group_b_s_m2"]}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
