#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-001：由权威 JSON 矩阵生成 CSV 视图（同构，供人工/diff）。
用法: python3 tools/traceability/gen_traceability_csv.py [--root <repo>]
输出: docs/traceability/TRACEABILITY_MATRIX.csv（稳定排序）
"""
from __future__ import annotations

import argparse
import csv
import json
import os

MATRIX_REL = "docs/traceability/TRACEABILITY_MATRIX.json"
CSV_REL = "docs/traceability/TRACEABILITY_MATRIX.csv"
COLS = ["module_id", "module_kind", "module_anchor",
        "science_id", "science_doc", "science_status",
        "algorithm_id", "algorithm_doc", "algorithm_status",
        "data_id", "data_status",
        "api_id", "api_status",
        "arch_id", "arch_status",
        "src_id", "src_path", "src_status",
        "test_id", "test_path", "test_status",
        "evidence_id", "evidence_status", "notes"]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    with open(os.path.join(root, MATRIX_REL), encoding="utf-8") as f:
        data = json.load(f)
    rows = []
    for row in sorted(data["modules"], key=lambda r: r["module_id"]):
        rows.append([str(row.get(c, "")) for c in COLS])
    out = os.path.join(root, CSV_REL)
    with open(out, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(COLS)
        w.writerows(rows)
    print(f"TRACEABILITY_CSV_WRITTEN rows={len(rows)} -> {CSV_REL}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
