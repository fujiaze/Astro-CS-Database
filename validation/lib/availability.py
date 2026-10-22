"""Which public entry points exist in the production tree right now.

The independent validation must never fabricate an observation.  For every
case the availability of the public entry point under test is established by
looking at the production sources (read-only) and, when the entry point
exists, by actually calling it.

This module only *reads* ``src/``; it never includes or links it.
"""

from __future__ import annotations

import re
from pathlib import Path

from .vcommon import REPO_ROOT

# The public free functions declared in include/acsd/contracts.hpp.
PUBLIC_ENTRY_POINTS = [
    "make_wcs", "to_fits", "from_fits_center", "from_fits_edge", "forward", "inverse",
    "derivative", "solid_angle_jacobian", "detector_footprint", "bind_wcs",
    "bind_observation_model", "footprint", "measure", "compute_overlap",
    "calibrate_observation", "regroup_observation", "build_catalogue_index",
    "make_catalogue_provider", "solve_astrometry", "fit_psf", "evaluate_psf",
    "measure_science_sources", "integrate_response", "fit_photometry", "apply_scale",
    "build_drizzle", "refine_geometry", "make_action", "apply", "transpose_apply", "compose",
    "make_covariance", "covariance_apply", "quadratic_form", "geometry_covariance_bound",
    "estimate_flux_information", "evaluate_scene_variance", "control_covariance",
    "reconstruct_scene_variance", "build_supported_triangulation", "build_sky_controls",
    "fit_relative_sky", "apply_sky", "select_samples", "solve_weights", "coadd",
    "build_export", "export_signal", "make_frames_provider", "open_product", "publish_product",
    "normalize", "mosaic", "export_image",
]

# Entry points that are pure value operations without a Result.
VALUE_ENTRY_POINTS = {"to_fits", "from_fits_center", "from_fits_edge"}


def _definition_pattern(name: str) -> re.Pattern[str]:
    # A definition looks like "Result<...> name(" or "FitsPixel1 to_fits(" at the
    # start of a line inside namespace acsd.
    return re.compile(r"^\s*(?:[A-Za-z_][\w:<>,\s\*&]*\s)" + re.escape(name) + r"\s*\(",
                      re.MULTILINE)


def implemented_entry_points() -> dict[str, bool]:
    """Map every public entry point onto 'a definition exists in src/'."""
    sources = sorted((REPO_ROOT / "src").rglob("*.cpp"))
    text = "\n".join(path.read_text(encoding="utf-8", errors="ignore") for path in sources)
    status: dict[str, bool] = {}
    for name in PUBLIC_ENTRY_POINTS:
        pattern = _definition_pattern(name)
        status[name] = bool(pattern.search(text))
    return status


def missing(entry_points: list[str]) -> list[str]:
    status = implemented_entry_points()
    return [name for name in entry_points if not status.get(name, False)]


def availability(entry_points: list[str]) -> dict[str, object]:
    status = implemented_entry_points()
    return {
        "entry_points": {name: status.get(name, False) for name in entry_points},
        "missing": [name for name in entry_points if not status.get(name, False)],
        "checked_sources": sorted(str(path.relative_to(REPO_ROOT))
                                  for path in (REPO_ROOT / "src").rglob("*.cpp")),
    }
