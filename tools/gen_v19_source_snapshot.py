#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_v19_source_snapshot.py — V19 source/ 快照 (full_first_party_after.zip +
source_manifest.csv + changed_only 由 git diff 生成)"""

from __future__ import annotations

import csv
import os
import subprocess
import sys
import zipfile


ROOT = r"F:\Astro dev\Astro CS Normalization Database"
REV = os.path.join(ROOT, "run", "temp", "v19_review")
SKIP_PARTS = {"build", "archive", "__pycache__", ".git", "third_party",
              "tests", "test", "examples", "experiments", "qualification",
              "worktrees", ".trae", "run", "testdata"}
SKIP_EXT = {".dll", ".exe", ".o", ".a", ".pyc", ".bak"}


def collect(base: str) -> list[str]:
    out = []
    for dp, _dn, fn in os.walk(base):
        parts = os.path.relpath(dp, ROOT).replace(os.sep, "/").split("/")
        if any(p in SKIP_PARTS for p in parts):
            continue
        for f in fn:
            if os.path.splitext(f)[1].lower() in SKIP_EXT:
                continue
            p = os.path.join(dp, f)
            if os.path.isfile(p):
                out.append(p)
    return out


def main() -> int:
    paths: list[str] = []
    for sub in ("lib", "docs", "tools"):
        paths.extend(collect(os.path.join(ROOT, sub)))
    paths = sorted(set(paths))

    zpath = os.path.join(REV, "source", "full_first_party_after.zip")
    os.makedirs(os.path.dirname(zpath), exist_ok=True)
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for p in paths:
            try:
                z.write(p, os.path.relpath(p, ROOT).replace(os.sep, "/"))
            except OSError:
                continue
    with open(os.path.join(REV, "source", "source_manifest.csv"), "w",
              newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["path", "size_bytes"])
        for p in paths:
            try:
                w.writerow([os.path.relpath(p, ROOT).replace(os.sep, "/"),
                            os.path.getsize(p)])
            except OSError:
                continue
    print(f"source files: {len(paths)}")
    print(f"zip bytes: {os.path.getsize(zpath)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
