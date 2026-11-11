#!/usr/bin/env python3
"""Check the assessment invariants of a published product (evidence, not a CI gate).

Invariants for objects that carry noise semantics:
  1. noise=unknown  => science is conditional (not bounded/None)
  2. complete=true  => no non-ok issue is listed
  3. science=conditional => complete=false

Usage: assessment_invariants_probe.py <product-dir> [<product-dir> ...]
Exit code is the number of violations (0 = clean).
"""
import glob
import json
import os
import sys

# Objects whose assessment has noise semantics.  Pure data arrays/indexes are
# excluded: their assessment is not a noise statement.  Everything else with an
# assessment is checked, including SupportMap/ErrorRecord/NoiseRoot.
SKIP_TYPES = ("Array", "Index")


def load_objects(root):
    for path in glob.glob(os.path.join(root, "objects", "*.json")):
        base = os.path.basename(path)
        if any(t in base for t in SKIP_TYPES):
            continue
        try:
            with open(path, "rb") as handle:
                payload = json.load(handle)
        except (OSError, ValueError):
            continue
        data = payload.get("data") if isinstance(payload, dict) else None
        if isinstance(data, dict) and isinstance(data.get("assessment"), dict):
            yield base, data["assessment"]


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    violations = 0
    for root in argv[1:]:
        checked = 0
        for base, assessment in load_objects(root):
            checked += 1
            issues = [
                issue.get("code")
                for issue in assessment.get("issues", [])
                if isinstance(issue, dict)
            ]
            if assessment.get("noise") == "unknown" and assessment.get("science") in (
                None,
                "bounded",
            ):
                print(f"{root}: {base}: unknown noise but science={assessment.get('science')}")
                violations += 1
            if assessment.get("complete") and any(
                code not in (None, "ok") for code in issues
            ):
                print(f"{root}: {base}: complete=true with issue codes {issues}")
                violations += 1
            if assessment.get("science") == "conditional" and assessment.get("complete"):
                print(f"{root}: {base}: conditional but complete=true")
                violations += 1
        print(f"{root}: {checked} model assessments checked")
    print(f"assessment invariants: {violations} violation(s)")
    return violations


if __name__ == "__main__":
    sys.exit(min(main(sys.argv), 255))
