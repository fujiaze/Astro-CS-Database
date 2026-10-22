#!/usr/bin/env python3
"""Run the whole independent validation tree and summarise the outcome.

Usage::

    python3 validation/run_all.py            # run every case
    python3 validation/run_all.py c01 u01    # run selected cases
    python3 validation/run_all.py --list     # list the cases

Each case script is self contained: it freezes the input, computes the
independent truth, runs the implementation under test (in memory public API
and the acsd-contract process driver), writes the comparison and the
manifest.  A case whose public entry point does not exist yet is reported as
``pending_implementation`` with the reason; no observed value is ever
fabricated.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

VALIDATION_ROOT = Path(__file__).resolve().parent
CASES = ["c01", "g01", "u01", "f01", "s01", "w01", "v01", "e01", "p01", "r01", "x01"]

# The drivers that are built from validation/native.
# G01 has no native driver: the public contract has no factory for the
# FootprintProvider, and an independent driver must not include the internal
# provider header, so the G01 observed side is reached through the
# acsd-contract process driver only.
DRIVERS = {
    "u01": "u01_driver",
    "c01": "c01_driver",
}


def build_drivers() -> tuple[bool, str]:
    script = VALIDATION_ROOT / "lib" / "build_native.sh"
    if not script.is_file():
        return False, "the native build script is missing"
    names = sorted(set(DRIVERS.values()))
    completed = subprocess.run(["bash", str(script), *names], capture_output=True, text=True)
    return completed.returncode == 0, (completed.stdout + completed.stderr)[-4000:]


def run_case(case: str) -> dict:
    script = VALIDATION_ROOT / case / f"truth_{case}.py"
    if not script.is_file():
        return {"case": case.upper(), "status": "missing_case_script"}
    start = time.monotonic()
    completed = subprocess.run([sys.executable, str(script)], capture_output=True, text=True,
                               cwd=str(VALIDATION_ROOT.parent))
    elapsed = time.monotonic() - start
    result = {"case": case.upper(), "exit_code": completed.returncode,
              "wall_seconds": elapsed,
              "stderr": completed.stderr[-2000:] if completed.returncode else ""}
    comparison = VALIDATION_ROOT / case / "comparison.json"
    if comparison.is_file():
        try:
            payload = json.loads(comparison.read_text(encoding="utf-8"))
            result["status"] = payload.get("status")
            result["functionals"] = len(payload.get("functionals", []))
            result["failures"] = payload.get("failures", [])
        except Exception as exc:
            result["status"] = "comparison_unreadable"
            result["error"] = str(exc)
    else:
        result["status"] = "no_comparison"
    if completed.returncode != 0 and "status" not in result:
        result["status"] = "script_failed"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cases", nargs="*", help="the cases to run (default: all)")
    parser.add_argument("--list", action="store_true", help="list the cases and exit")
    parser.add_argument("--skip-build", action="store_true",
                        help="do not build the native drivers")
    args = parser.parse_args()

    if args.list:
        for case in CASES:
            print(case)
        return 0

    selected = [case.lower() for case in (args.cases or CASES)]
    unknown = [case for case in selected if case not in CASES]
    if unknown:
        print(f"unknown cases: {unknown}", file=sys.stderr)
        return 2

    build_log = ""
    if not args.skip_build:
        ok, build_log = build_drivers()
        if not ok:
            print("the native drivers could not be built; the affected cases stay "
                  "pending_implementation", file=sys.stderr)
            print(build_log, file=sys.stderr)

    results = []
    for case in selected:
        print(f"=== {case.upper()} ===", flush=True)
        results.append(run_case(case))

    summary = {
        "validation_summary": {
            "cases": results,
            "counts": {
                "pass": sum(1 for r in results if r.get("status") == "pass"),
                "fail": sum(1 for r in results if r.get("status") == "fail"),
                "pending_implementation": sum(1 for r in results
                                              if r.get("status") == "pending_implementation"),
                "other": sum(1 for r in results
                             if r.get("status") not in
                             ("pass", "fail", "pending_implementation")),
            },
            "environment": {"python": sys.version.split()[0]},
        }
    }
    out = VALIDATION_ROOT / "run_summary.json"
    out.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    print("\n=== summary ===")
    for result in results:
        print(f"{result['case']:6s} {str(result.get('status')):26s} "
              f"{result.get('wall_seconds', 0.0):6.1f}s")
    counts = summary["validation_summary"]["counts"]
    print(f"pass={counts['pass']} fail={counts['fail']} "
          f"pending_implementation={counts['pending_implementation']} "
          f"other={counts['other']}")
    print(f"summary written to {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
