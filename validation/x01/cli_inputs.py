"""The frozen CLI inputs of X01 (the end-to-end case).

The files written here are the inputs of the three commands
(``acsd normalize`` / ``mosaic`` / ``export``).  They are built from the
frozen synthetic observation of the case; nothing here is produced by the
implementation under test.  Every file is validated against the published
schemas before the commands are run, so a schema violation is reported as a
validation failure and never as a scientific result.
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any, Dict, List

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS

REPO_ROOT = Path(__file__).resolve().parents[2]
CASE_ROOT = Path(__file__).resolve().parent
CLI_DIR = CASE_ROOT / "input" / "cli"

WAVELENGTH_MIN_NM = 400.0
WAVELENGTH_MAX_NM = 700.0
WAVELENGTH_POINTS = 301


def throughput_of(group: str, wavelength: np.ndarray) -> np.ndarray:
    if group == "A":
        return 0.6 * np.exp(-0.5 * ((wavelength - 520.0) / 60.0) ** 2)
    return 0.45 * np.exp(-0.5 * ((wavelength - 610.0) / 45.0) ** 2)


def source_field() -> List[Dict[str, Any]]:
    """The frozen source field of the case (the same one the frames use)."""
    field = [{"kind": "bright", "centre_pixel": [63.5, 64.5], "weight": 1.0}]
    for centre_x, centre_y, weight in ((24.5, 30.5, 0.80), (96.5, 22.5, 0.90),
                                       (30.5, 100.5, 0.70), (92.5, 98.5, 1.00),
                                       (14.5, 72.5, 0.75), (110.5, 66.5, 0.85)):
        field.append({"kind": "star", "centre_pixel": [centre_x, centre_y], "weight": weight})
    return field


def reference_wcs() -> WCS:
    """The WCS of the A group, read back from the frozen frame."""
    with fits.open(CASE_ROOT / "input" / "frames" / "frame_00_A.fits") as handle:
        return WCS(handle[0].header)


def sky_positions() -> List[Dict[str, Any]]:
    """The sky position of every source, from the frozen WCS."""
    wcs = reference_wcs()
    entries = []
    for index, source in enumerate(source_field()):
        pixel = source["centre_pixel"]
        x, y = wcs.all_pix2world(pixel[0], pixel[1], 0)
        entries.append({
            "source_id": 1000 + index,
            "ra_deg": float(x),
            "dec_deg": float(y),
            "kind": source["kind"],
            "weight": source["weight"],
            "centre_pixel": pixel,
        })
    return entries


def magnitude_of(weight: float) -> float:
    """A brightness scale that is consistent with the construction.

    The absolute zero point is not part of the synthetic observation, so the
    magnitude is only a monotone label of the relative brightness; the
    verification compares the ratios, not the absolute magnitudes.
    """
    return -2.5 * math.log10(max(weight, 1e-6))


def catalogue_document() -> Dict[str, Any]:
    positions = sky_positions()
    return {
        "source_ids": [entry["source_id"] for entry in positions],
        # eight astrometry columns, all in the units of the catalogue:
        # ra (rad), dec (rad), epoch (year), pmra (rad/yr), pmdec (rad/yr),
        # parallax (rad), radial velocity (m/s) and the radial velocity flag
        "astrometry": [[math.radians(entry["ra_deg"]), math.radians(entry["dec_deg"]),
                        2016.0, 0.0, 0.0, 0.0, 0.0, 0.0]
                       for entry in positions],
        "brightness_mag": [magnitude_of(entry["weight"]) for entry in positions],
        "xp_coefficients": [[1.0] + [0.0] * 2 for _ in positions],
        "xp_basis": {
            "knots_nm": [400.0, 550.0, 700.0],
            "basis_coefficients": [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
            "degree": 2,
            "coefficient_count": 3,
            "support_nm": {"lower": 400.0, "upper": 700.0},
        },
        "build": {
            "sky_order": 5,
            "max_stars_per_cell": 64,
            "neighbors_per_anchor": 12,
            "quads_per_anchor": 8,
            "quad_baseline_rad": {"lower": 1.0e-5, "upper": 1.0e-2},
        },
    }


def response_document(group: str) -> Dict[str, Any]:
    wavelength = np.linspace(WAVELENGTH_MIN_NM, WAVELENGTH_MAX_NM, WAVELENGTH_POINTS)
    return {
        "convention": "photon_throughput",
        "wavelength_nm": [float(value) for value in wavelength],
        "throughput": [float(value) for value in throughput_of(group, wavelength)],
    }


def calibration_document() -> Dict[str, Any]:
    """The calibration of the raw frames.

    The bias and the dark are zero by construction of the synthetic frames;
    the dark level is folded into the sky level of the frames, so the
    calibration only carries the exposure and the normalisation.
    """
    pixels = 128 * 128
    return {
        # the bias is zero by construction, the flat is unity and the dark
        # carries the 12 electrons per second of the construction, converted
        # to ADU with the gain of 2.5
        "bias": [0.0] * pixels,
        "dark": [12.0 / 2.5] * pixels,
        "flat": [1.0] * pixels,
        "exposure_seconds": 100.0,
        "dark_exposure_seconds": 100.0,
        "flat_normalization": 1.0,
        "dark_contains_bias": True,
        "gain_e_per_adu": 2.5,
        "read_rms_e": 3.0,
    }


def numeric_budget() -> Dict[str, Any]:
    return {
        "tolerance": {"absolute": 1e-12, "relative": 1e-12, "absolute_floor": 0.0,
                      "unit": {"scale": "1", "sr_power": 0, "pixel_power": 0, "nm_power": 0,
                               "multiplier": 1.0, "linear_power": 0}},
        "max_iterations": 128,
        "arithmetic": {"mode": "outward", "initial_bits": 128, "max_bits": 256,
                       "allow_target_fallback": False},
    }


def estimator(identifier: str, quantity: str, unit_scale: str = "1") -> Dict[str, Any]:
    return {
        "id": identifier,
        "quantity": quantity,
        "unit": {"scale": unit_scale, "sr_power": 0, "pixel_power": 0, "nm_power": 0,
                 "multiplier": 1.0, "linear_power": 0},
        "condition": {"kind": "fixed_geometry", "estimator": identifier,
                      "assumptions": [], "fixed_objects": []},
        "reference_sed": None,
    }


def solver_request() -> Dict[str, Any]:
    center = target_wcs_model()["c"]
    return {
        # The blind B group needs a search region and a scale interval; the
        # frozen field is centred on the frame tangent point.
        "regions": [{"center": {"nx": center[0], "ny": center[1], "nz": center[2]},
                     "radius_rad": 0.01}],
        # the scale interval must exclude zero; the frozen scale of the
        # construction is 4.848e-06 rad per pixel
        "scale_rad_pixel": {"lower": 1.0e-6, "upper": 1.0e-4},
        "max_candidates": 64,
        "max_hypotheses": 8,
        "sip_order": 3,
        "detect_snr": 5.0,
        "match_radius_pixel": 3.0,
        "min_matches": 4,
        "quad_descriptor_tolerance": 0.2,
        "budget": numeric_budget(),
    }


def psf_request() -> Dict[str, Any]:
    return {
        "family": "gaussian",
        "spatial_degree": 0,
        "initial_width_pixel": 1.35,
        "tail_absolute_flux": 1e-4,
        "window_radius_pixel": 6.0,
        "budget": numeric_budget(),
    }


def drizzle_request() -> Dict[str, Any]:
    return {
        "order": 6,
        "pixfrac": 1.0,
        "budget": {
            "requested_functionals": [],
            "per_leaf_absolute_area": [],
            "area_absolute_sr": 1e-12,
            "normalizer_min_sr": 0.0,
            "max_work": 1000000,
            "max_bytes": 16777216,
            "max_depth": 32,
            "arithmetic": {"mode": "outward", "initial_bits": 64, "max_bits": 256,
                           "allow_target_fallback": False},
        },
    }


def photometry_request() -> Dict[str, Any]:
    return {
        "responses": [],
        "reference": None,
        "basis": {"kind": "constant", "degree": 0,
                  "normalization_domain": {"lower": [0.0, 0.0], "upper": [1.0, 1.0]},
                  "center": [0.0, 0.0], "scale": [1.0, 1.0], "weighted_mean": None},
        "frames": [],
        "application_id": "x01-scale",
        "solve": {"relative_residual": 1e-8, "absolute_residual": 1e-12,
                  "rank_relative": 1e-10, "max_iterations": 64, "dense_max_bytes": 16777216,
                  "huber": False, "huber_delta": 0.0},
        "gradient_tolerance": 1e-10,
        "step_tolerance": 1e-12,
        "max_outer_iterations": 32,
        # The first-phase photometry implements the flux-domain joint
        # Gaussian latent; the low-count raw Poisson likelihood is an explicit
        # unsupported capability (photometry.fit.md).  X01 requests the
        # supported estimator instead of an unimplemented one.
        "likelihood": {"kind": "joint_flux_gaussian",
                        "estimator": "x01-likelihood",
                        "assumptions": [], "fixed_objects": []},
    }


def information_request() -> List[Dict[str, Any]]:
    return []


def tangent_basis(crval_deg, lonpole_deg: float):
    """The canonical tangent basis (c, e, north) of the foundation equation (3).

    c is the tangent point, east is the equatorial tangent at the tangent
    point and north is the meridian direction at the tangent point:

        c    = (cos d cos a, cos d sin a, sin d)
        e    = (-sin a, cos a, 0)
        north = (-sin d cos a, -sin d sin a, cos d)
    """
    alpha0 = math.radians(crval_deg[0])
    delta0 = math.radians(crval_deg[1])
    centre = np.array([math.cos(delta0) * math.cos(alpha0),
                       math.cos(delta0) * math.sin(alpha0),
                       math.sin(delta0)])
    east = np.array([-math.sin(alpha0), math.cos(alpha0), 0.0])
    north = np.array([-math.sin(delta0) * math.cos(alpha0),
                      -math.sin(delta0) * math.sin(alpha0),
                      math.cos(delta0)])
    return centre, east, north


def canonical_cd(cd_deg, crval_deg: float, lonpole_deg: float):
    """The canonical CD of the tangent plane (the FITS LONPOLE rule)."""
    cd = np.array(cd_deg, dtype=float)
    if abs(crval_deg - 90.0) < 1e-12 and abs(lonpole_deg) < 1e-12:
        return -cd
    if abs(crval_deg + 90.0) < 1e-12 and abs(abs(lonpole_deg) - 180.0) < 1e-12:
        return -cd
    return cd


def wcs_model_data(detector_frame: str = "detector") -> Dict[str, Any]:
    """The WcsModel of the A group, from the frozen frame header."""
    with fits.open(CASE_ROOT / "input" / "frames" / "frame_00_A.fits") as handle:
        header = handle[0].header
    crpix = (float(header["CRPIX1"]), float(header["CRPIX2"]))
    crval = (float(header["CRVAL1"]), float(header["CRVAL2"]))
    cd_deg = [[float(header["CD1_1"]), float(header["CD1_2"])],
              [float(header["CD2_1"]), float(header["CD2_2"])]]
    lonpole = 180.0
    cd = canonical_cd(cd_deg, crval[1], lonpole)
    centre, east, north = tangent_basis(crval, lonpole)
    radians = math.pi / 180.0
    cd_rad = [[cd[0][0] * radians, cd[0][1] * radians],
              [cd[1][0] * radians, cd[1][1] * radians]]
    return {
        "crpix_fits1": [crpix[0], crpix[1]],
        "cd_rad_per_pixel": [cd_rad[0][0], cd_rad[0][1], cd_rad[1][0], cd_rad[1][1]],
        "c": [float(value) for value in centre],
        "e": [float(value) for value in east],
        "north": [float(value) for value in north],
        # the frame model must cover the whole detector, whose pixel edges
        # run from -0.5 to width - 0.5
        "domain": {"lower": [-0.5, -0.5], "upper": [127.5, 127.5]},
        "detector_frame": detector_frame,
    }


def normalize_scientific() -> Dict[str, Any]:
    return {
        "command": "normalize",
        "request": {
            "astrometry": solver_request(),
            "psf": psf_request(),
            "photometry": photometry_request(),
            "drizzle": drizzle_request(),
            "information": information_request(),
            "allow_relative_uncalibrated": True,
        },
        # the A group carries its WCS, the B group must be blind solved
        "frame_options": [
            {"frame": f"x01-{index:02d}",
             "supplied_wcs": wcs_model_data(f"x01-{index:02d}"), "regroup": None}
            for index in range(4)
        ],
    }


def mosaic_scientific() -> Dict[str, Any]:
    return {
        "command": "mosaic",
        "request": {
            "sky": {
                "groups": [],
                "basis": {"kind": "constant", "degree": 0,
                  "normalization_domain": {"lower": [0.0, 0.0], "upper": [1.0, 1.0]},
                  "center": [0.0, 0.0], "scale": [1.0, 1.0], "weighted_mean": None},
                "gauge": {"kind": "reference_frame", "reference": "x01-00", "weights": None},
                "T": None,
                "H": None,
                "control_noise": None,
                "solve": {"relative_residual": 1e-8, "absolute_residual": 1e-12,
                          "rank_relative": 1e-10, "max_iterations": 64,
                          "dense_max_bytes": 16777216, "huber": False, "huber_delta": 0.0},
                "allow_prediction": False,
            },
            "coaddition": {
                "target": estimator("x01-coadd", "brightness"),
                "mixed_observation": False,
                "weights": "automatic",
                "rejection": {"kind": "dq_only", "alpha": 0.0,
                              "external_fixed_covariance": None, "common_mean_proof": ""},
                "quantities": [],
                "solve": {"relative_residual": 1e-8, "absolute_residual": 1e-12,
                          "rank_relative": 1e-10, "max_iterations": 64,
                          "dense_max_bytes": 16777216, "huber": False, "huber_delta": 0.0},
                "groups": [],
            },
        },
    }



def target_wcs_model() -> Dict[str, Any]:
    """The tangent-plane model of the frozen frame (the export target)."""
    with fits.open(CASE_ROOT / "input" / "frames" / "frame_00_A.fits") as handle:
        header = handle[0].header
    ra = math.radians(float(header["CRVAL1"]))
    dec = math.radians(float(header["CRVAL2"]))
    center = np.array([math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec)],
                      dtype=float)
    east = np.cross(np.array([0.0, 0.0, 1.0]), center)
    east = east / np.linalg.norm(east)
    north = np.cross(center, east)
    cd = np.array([[header["CD1_1"], header["CD1_2"]],
                   [header["CD2_1"], header["CD2_2"]]], dtype=float) * (math.pi / 180.0)
    return {
        "crpix_fits1": [float(header["CRPIX1"]), float(header["CRPIX2"])],
        "cd_rad_per_pixel": [float(cd[0, 0]), float(cd[0, 1]), float(cd[1, 0]), float(cd[1, 1])],
        "c": [float(center[0]), float(center[1]), float(center[2])],
        "e": [float(east[0]), float(east[1]), float(east[2])],
        "north": [float(north[0]), float(north[1]), float(north[2])],
        "domain": {"lower": [-0.5, -0.5], "upper": [127.5, 127.5]},
        "detector_frame": "x01",
    }


def export_scientific() -> Dict[str, Any]:
    return {
        "command": "export",
        "request": {
            "projection": {
            "target": {"model": target_wcs_model(), "width": 128, "height": 128},
            "quantity": "supported_flux",
            "support_policy": "full_leaf_approx",
            "assume_uniform_uncovered": False,
            "output_dtype": "f64",
            "budget": {
                "requested_functionals": [],
                "per_leaf_absolute_area": [],
                "area_absolute_sr": 1e-12,
                "normalizer_min_sr": 0.0,
                "max_work": 1000000,
                "max_bytes": 16777216,
                "max_depth": 32,
                "arithmetic": {"mode": "outward", "initial_bits": 64, "max_bits": 256,
                               "allow_target_fallback": False},
            },
            },
        },
    }


def resources() -> Dict[str, Any]:
    return {
        "bytes": 268435456,
        "work": 10000000,
        "spill_bytes": 0,
        "threads": 1,
        "max_depth": 32,
        "max_precision_bits": 256,
        "scratch_directory": ".work",
    }


def normalize_config(frame_ids: List[str]) -> Dict[str, Any]:
    return {
        "schema": "acsd.config/2",
        "command": "normalize",
        "inputs": [
            {"path": f"frames/frame_{index:02d}_{'A' if index < 4 else 'B'}.fits",
             "frame_id": frame_id,
             "response": f"response_{'A' if index < 4 else 'B'}.json",
             "calibration": "calibration.json"}
            for index, frame_id in enumerate(frame_ids)
        ],
        "catalogue": "catalogue.json",
        "scientific": "normalize.scientific.json",
        "output": "products",
        "resources": resources(),
        "product_profile": "full",
        "publication": {"signal_dtype": "f64", "tile_log2_width": 6, "durable": False},
    }


def mosaic_config() -> Dict[str, Any]:
    return {
        "schema": "acsd.config/2",
        "command": "mosaic",
        "inputs": [{"path": f"products/x01-0{index}",
                    "expected_hash": "sha256:" + "0" * 64} for index in range(4)],
        "publication": {"signal_dtype": "f64", "tile_log2_width": 6, "durable": False},
        "scientific": "mosaic.scientific.json",
        "output": "products/mosaic",
        "resources": resources(),
        "product_profile": "full",
    }


def export_config() -> Dict[str, Any]:
    return {
        "schema": "acsd.config/2",
        "command": "export",
        "inputs": [{"path": "products/mosaic",
                    "expected_hash": "sha256:" + "0" * 64}],
        "scientific": "export.scientific.json",
        "output": "products/export",
        "resources": resources(),
        "product_profile": "full",
    }


def write_inputs() -> Dict[str, Any]:
    """Write every frozen CLI input and return the manifest of the files."""
    CLI_DIR.mkdir(parents=True, exist_ok=True)
    (CLI_DIR / "frames").mkdir(exist_ok=True)
    for index in range(8):
        group = "A" if index < 4 else "B"
        source = CASE_ROOT / "input" / "frames" / f"frame_{index:02d}_{group}.fits"
        target = CLI_DIR / "frames" / f"frame_{index:02d}_{group}.fits"
        target.write_bytes(source.read_bytes())

    documents = {
        "catalogue.json": catalogue_document(),
        "response_A.json": response_document("A"),
        "response_B.json": response_document("B"),
        "calibration.json": calibration_document(),
        "normalize.scientific.json": normalize_scientific(),
        "mosaic.scientific.json": mosaic_scientific(),
        "export.scientific.json": export_scientific(),
        "normalize.config.json": normalize_config([f"x01-{index:02d}" for index in range(8)]),
        "mosaic.config.json": mosaic_config(),
        "export.config.json": export_config(),
    }
    for name, document in documents.items():
        (CLI_DIR / name).write_text(json.dumps(document, indent=1) + "\n", encoding="utf-8")
    return {"directory": str(CLI_DIR), "files": sorted(documents)}


if __name__ == "__main__":
    manifest = write_inputs()
    print(json.dumps(manifest, indent=1))
