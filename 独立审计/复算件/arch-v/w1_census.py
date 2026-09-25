#!/usr/bin/env python3
"""AUDIT-06 arch-v W1: reference census for the three phase-scheduler classes.

Buckets are mutually exclusive, resolved in this order:
  SELF       -> the class's own public header + its own translation unit
  TEST       -> eng/tests/** or any */tests/**
  GATE       -> eng/ci/** or eng/tools/**   (registry / static-gate text, not compiled)
  PRODUCTION -> everything else             (candidate real consumers)

Own script; imports nothing from the repository. Read-only.
"""
import re
import subprocess
import sys
from collections import defaultdict

REPO = r"F:\Astro dev\Astro CS Normalization Database"

CLASSES = {
    "NormalizeWorkflowScheduler": (
        "lib/include/astrocs/core/normalize_workflow.h",
        "lib/infrastructure/scheduler/src/normalize_workflow.cpp",
    ),
    "MosaicWindowScheduler": (
        "lib/include/astrocs/core/mosaic_window.h",
        "lib/infrastructure/scheduler/src/mosaic_window.cpp",
    ),
    "ExportStreamScheduler": (
        "lib/include/astrocs/core/export_stream.h",
        "lib/infrastructure/scheduler/src/export_stream.cpp",
    ),
}


def git_grep_c(pattern, extra=()):
    cmd = ["git", "grep", "-c", "-e", pattern, "--"] + list(extra)
    r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    out = []
    for line in r.stdout.splitlines():
        m = re.match(r"^(.+?):(\d+)$", line)
        if m:
            out.append((m.group(1), int(m.group(2))))
    return out


def bucket(path, self_files):
    if path in self_files:
        return "SELF"
    if path.startswith("eng/tests/") or "/tests/" in path:
        return "TEST"
    if path.startswith("eng/ci/") or path.startswith("eng/tools/"):
        return "GATE"
    if path.startswith("docs/"):
        return "DOC"
    return "PRODUCTION"


def main():
    only = sys.argv[1:] or list(CLASSES)
    for cls in only:
        hdr, tu = CLASSES[cls]
        hits = git_grep_c(cls)
        tally = defaultdict(int)
        per_file = defaultdict(list)
        for path, n in hits:
            b = bucket(path, {hdr, tu})
            tally[b] += n
            per_file[b].append((path, n))
        print("=" * 70)
        print(f"CLASS {cls}")
        print(f"  SELF header : {hdr}")
        print(f"  SELF TU     : {tu}")
        for b in ("SELF", "TEST", "GATE", "DOC", "PRODUCTION"):
            print(f"  {b:11s} hits={tally.get(b,0):3d}  files={len(per_file.get(b,[]))}")
        for b in ("PRODUCTION", "TEST", "GATE"):
            if per_file.get(b):
                print(f"  --- {b} files ---")
                for path, n in sorted(per_file[b]):
                    print(f"      {n:3d}  {path}")


main()
