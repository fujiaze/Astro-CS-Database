#!/usr/bin/env python3
"""Freeze the observed harness output of an M5 process driver into a comparison.

Usage: compare.py CASE_DIR

The driver writes observed.json (the harness document on stdout).  This script
requires at least one check, requires every recorded check to pass, and writes
comparison.json with the real counters.  A driver that only "ran" is never a
pass.
"""
import json
import sys
from pathlib import Path


def main() -> int:
    directory = Path(sys.argv[1]).resolve()
    observed = json.loads((directory / "observed.json").read_text(encoding="utf-8"))
    checks = observed.get("checks", [])
    failures = observed.get("failures", [])
    attempts = int(observed.get("attempts", 0))
    successes = int(observed.get("successes", 0))
    status = "pass" if attempts > 0 and successes == attempts and not failures else "fail"
    comparison = {
        "case": observed.get("case"),
        "status": status,
        "attempts": attempts,
        "successes": successes,
        "failures": failures,
        "values": observed.get("values", {}),
        "resources": observed.get("resources", {}),
        "notes": observed.get("notes", []),
        "checks": checks,
        "source": observed.get("source"),
    }
    (directory / "comparison.json").write_text(
        json.dumps(comparison, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{directory.name}: {status} ({successes}/{attempts})")
    return 0 if status == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
