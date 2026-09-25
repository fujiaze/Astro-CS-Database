# -*- coding: utf-8 -*-
"""append approved rows to D4-多侧默认值候选.csv (UTF-8 BOM, quoted).

Usage: python append_rows.py rows_partNN.json
Reads a JSON list of {11 columns} and appends them, preserving column order.
"""
from __future__ import annotations

import csv
import io
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.abspath(os.path.join(
    HERE, "..", "..", "raw", "D4-多侧默认值候选.csv"))
COLS = ["键", "侧1登记值:位点", "侧2出厂模板", "侧3CLI骨架", "侧4机器合同",
        "侧5代码兜底", "侧6登记册", "命中判据", "是否已有门覆盖", "定级建议",
        "备注（保守方向与影响面）"]


def main():
    src = sys.argv[1]
    rows = json.load(io.open(src, encoding="utf-8"))
    new = not os.path.isfile(CSV)
    with io.open(CSV, "a", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f, quoting=csv.QUOTE_ALL, lineterminator="\n")
        if new:
            w.writerow(COLS)
        for r in rows:
            w.writerow([str(r.get(c, "")) for c in COLS])
    with io.open(CSV, encoding="utf-8-sig", newline="") as f:
        n = len(list(csv.reader(f))) - 1
    sys.stdout.write("appended %d rows -> total %d data rows\n" % (len(rows), n))


if __name__ == "__main__":
    main()
