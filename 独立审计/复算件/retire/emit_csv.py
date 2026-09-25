# -*- coding: utf-8 -*-
"""AUD-404 CSV 追加器。

用法：python emit_csv.py <batch.py> [<batch2.py> ...]
每个 batch.py 暴露 ROWS = [[9 字段], ...]；首个文件另暴露 MODE="init" 或 "append"。
列：对象/类别/四档定位/引用面计数/是否被某条门或合同引用/处置/连带改动/风险/置信
"""
import csv
import importlib.util
import os
import sys

OUT = (r"独立审计/证据"
       r"\AUD-404-退役与死代码清单.csv")
HEADER = ["对象", "类别", "四档定位", "引用面计数", "是否被某条门或合同引用",
          "处置", "连带改动", "风险", "置信"]

sys.stdout.reconfigure(encoding="utf-8")


def load(path):
    spec = importlib.util.spec_from_file_location(
        "b_" + os.path.basename(path).replace(".", "_"), os.path.abspath(path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return getattr(mod, "MODE", "append"), mod.ROWS


fh = None
try:
    for i, path in enumerate(sys.argv[1:]):
        mode, rows = load(path)
        if i == 0 and mode == "init":
            fh = open(OUT, "w", encoding="utf-8-sig", newline="")
        elif fh is None:
            fh = open(OUT, "a", encoding="utf-8-sig", newline="")
        w = csv.writer(fh, quoting=csv.QUOTE_ALL, lineterminator="\r\n")
        if i == 0 and mode == "init":
            w.writerow(HEADER)
        for r in rows:
            assert len(r) == 9, "%s: %r" % (path, r[0])
            w.writerow(r)
        print("%-14s -> %2d rows" % (os.path.basename(path), len(rows)))
finally:
    if fh:
        fh.close()

with open(OUT, encoding="utf-8-sig", newline="") as f:
    n = len(list(csv.reader(f))) - 1
print("TOTAL data rows =", n)
