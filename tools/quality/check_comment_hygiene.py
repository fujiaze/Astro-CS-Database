#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_comment_hygiene.py — V19R2 comment-hygiene gate.

Scans first-party production C/C++ sources for forbidden history markers:
  V[0-9]+ / R[0-9]+ / MICROFIX / 控制包 / 审计 / 骨架版本 / 号计划
with whitelist: FITS/HiPS/protocol/scientific model versions and
lowercase vendor versions (astrocs-upm-v2, NoiseWeightModelV1).

Output: reports/v19r2/evidence/quality/comment_check.json and
reports/v19r2/comment_hygiene.md.
"""

from __future__ import annotations

import csv
import json
import os
import re
import subprocess
import sys


ROOT = r"F:\Astro dev\Astro CS Normalization Database"
OUT = os.path.join(ROOT, "reports", "v19r2")

FORBIDDEN = re.compile(
    r"(?<![A-Za-z0-9_-])(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+)?|R\d+|"
    r"MICROFIX)(?![A-Za-z0-9_-])|控制包|审计|骨架版本|号计划")

# Formal/scientific version contexts that are allowed.
WHITELIST = re.compile(
    r"(?:FITS|HiPS|IVOA|schema|protocol|astrocs-upm-v\d+|"
    r"NoiseWeightModelV1|model_hash|format|astrocs-stage2|"
    r"astrocs_adaptive|wbpp_\d+_\d+_\d+|upm_v\d+|hiss_v\d+)",
    re.IGNORECASE)


def tracked() -> list[str]:
    r = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace",
                       timeout=120)
    return r.stdout.splitlines()


def line_is_allowed(line: str) -> bool:
    # 整行是 formal version / protocol 声明时允许
    return bool(WHITELIST.search(line))


def main() -> int:
    files = [p for p in tracked()
             if p.startswith("lib/") and p.endswith((".cpp", ".h", ".hpp",
                                                     ".c", ".cc", ".hh"))
             and not any(x in p.split("/")
                         for x in ("build", "build2", "_deps", "CMakeFiles",
                                   "archive", "third_party", "worktrees"))]
    violations: list[dict] = []
    for p in files:
        path = os.path.join(ROOT, p)
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
        except OSError:
            continue
        for i, raw in enumerate(lines, 1):
            if "//" not in raw:
                continue
            if line_is_allowed(raw):
                continue
            for m in FORBIDDEN.finditer(raw):
                violations.append({
                    "file": p, "line": i, "match": m.group(0),
                    "text": raw.strip()[:160],
                })
                break  # 每行记一次即可
    os.makedirs(os.path.join(OUT, "evidence", "quality"), exist_ok=True)
    with open(os.path.join(OUT, "evidence", "quality",
                           "comment_check.json"), "w", encoding="utf-8") as f:
        json.dump({"files_scanned": len(files),
                   "violation_lines": len(violations),
                   "violations": violations}, f, ensure_ascii=False, indent=1)
    by_file: dict[str, int] = {}
    for v in violations:
        by_file[v["file"]] = by_file.get(v["file"], 0) + 1
    md = ["# Comment Hygiene Report (V19R2)",
          "", f"- files scanned: {len(files)}",
          f"- violation lines: {len(violations)}",
          f"- files with violations: {len(by_file)}", ""]
    for p, n in sorted(by_file.items(), key=lambda kv: -kv[1]):
        md.append(f"- {n:4d}  {p}")
    with open(os.path.join(OUT, "comment_hygiene.md"), "w",
              encoding="utf-8") as f:
        f.write("\n".join(md) + "\n")
    print(f"comment hygiene: scanned={len(files)} "
          f"violations={len(violations)} files={len(by_file)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
