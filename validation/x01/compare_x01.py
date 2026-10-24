"""The X01 truth comparison from the measured point sources.

The oracle is the frozen truth: the measured pixel positions and integrated
fluxes of every frame are compared with the injected source centres and the
expected photon rate of the group (the bright source carries the emission line,
the field stars scale with their weight).  The trace is produced by the
normalize command; the comparison never reads implementation internals.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

CASE_ROOT = Path(__file__).resolve().parent
TRUTH = CASE_ROOT / "truth" / "x01_truth.json"
TRACE = CASE_ROOT / "observed" / "normalize_trace.jsonl"
OUTPUT = CASE_ROOT / "observed" / "compare.json"

sys.path.insert(0, str(CASE_ROOT))
import cli_inputs  # noqa: E402
import truth_x01  # noqa: E402


def expected_rates(group: str) -> dict[int, float]:
    """The injected photon rate of every source of the frozen field."""
    field = cli_inputs.source_field()
    wavelength = np.linspace(400.0, 700.0, 3001)
    throughput = cli_inputs.throughput_of(group, wavelength)
    continuum = truth_x01.continuum_sed(wavelength, 1.0e-18)
    bright_sed = continuum + truth_x01.line_sed(wavelength, 656.3, 1.5, 6.7e-18)
    bright = truth_x01.photon_integral(wavelength, bright_sed, throughput)
    star = truth_x01.photon_integral(wavelength, continuum, throughput)
    rates: dict[int, float] = {}
    for index, source in enumerate(field):
        rates[1000 + index] = bright if source["kind"] == "bright" else star * source["weight"]
    return rates


def main() -> int:
    trace_path = Path(sys.argv[1]) if len(sys.argv) > 1 else TRACE
    truth = json.loads(TRUTH.read_text(encoding="utf-8"))
    frames = {int(frame["frame"]): frame for frame in truth["frames"]}
    records: list[dict] = []
    if trace_path.is_file():
        for line in trace_path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line.startswith("{"):
                continue
            record = json.loads(line)
            if record.get("step") == "measured_stars":
                records.append(record)
    per_frame = []
    for record in records:
        try:
            index = int(str(record["frame"]).split("-")[-1])
        except (KeyError, ValueError):
            continue
        frame = frames.get(index)
        if frame is None:
            continue
        group = frame["group"]
        rates = expected_rates(group)
        field = cli_inputs.source_field()
        measured = {int(star["id"]): star for star in record.get("stars", [])}
        offsets = []
        truth_flux = []
        measured_flux = []
        for source_index, source in enumerate(field):
            source_id = 1000 + source_index
            star = measured.get(source_id)
            if star is None:
                continue
            expected_x, expected_y = source["centre_pixel"]
            offsets.append(float(np.hypot(star["x"] - expected_x, star["y"] - expected_y)))
            truth_flux.append(rates[source_id])
            measured_flux.append(float(star["flux"]))
        rms = float(np.sqrt(np.mean(np.square(offsets)))) if offsets else float("inf")
        signal_error = float("inf")
        if truth_flux:
            truth_peak = max(truth_flux)
            measured_peak = max(measured_flux)
            errors = [abs((m / measured_peak) - (t / truth_peak)) / (t / truth_peak)
                      for t, m in zip(truth_flux, measured_flux) if t > 0.0]
            if errors:
                signal_error = float(max(errors))
        per_frame.append({"frame": record["frame"], "group": group,
                          "astrometry_rms_pixel": rms, "signal_relative_error": signal_error,
                          "sources": len(offsets)})
    blind = [entry["astrometry_rms_pixel"] for entry in per_frame if entry["group"] == "B"]
    all_offsets = [entry["astrometry_rms_pixel"] for entry in per_frame]
    rms = float(np.sqrt(np.mean(np.square(all_offsets)))) if all_offsets else float("inf")
    blind_rms = float(np.sqrt(np.mean(np.square(blind)))) if blind else float("inf")
    signal_error = max((entry["signal_relative_error"] for entry in per_frame), default=float("inf"))
    case = json.loads((CASE_ROOT / "case.json").read_text())
    rms_tolerance = float(case["tolerance"]["astrometry_rms_pixel"])
    signal_tolerance = float(case["tolerance"]["signal_relative"])
    result = {
        "status": "pass" if all_offsets and not blind and False else "measured",
        "frames": per_frame,
        "astrometry_rms_pixel": rms,
        "blind_solve_rms_pixel": blind_rms,
        "astrometry_tolerance_pixel": rms_tolerance,
        "blind_solve": bool(blind and blind_rms <= rms_tolerance),
        "signal_relative_error": signal_error,
        "signal_tolerance": signal_tolerance,
        "source_recovered": bool(all_offsets and signal_error <= signal_tolerance),
        "trace": str(trace_path),
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, indent=1) + "\n", encoding="utf-8")
    print(json.dumps({key: result[key] for key in
                      ("astrometry_rms_pixel", "blind_solve_rms_pixel",
                       "signal_relative_error", "blind_solve", "source_recovered", "frames")},
                     indent=1))
    return 0 if result["blind_solve"] and result["source_recovered"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
