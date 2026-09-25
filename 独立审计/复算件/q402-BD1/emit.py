# -*- coding: utf-8 -*-
"""AUD-402 / Q=BD1 判读 CSV 装配器。用法: python emit.py <rows_file>（append 到目标 CSV）"""
import sys, csv, os, io
sys.stdout.reconfigure(encoding='utf-8')

OUT = r"独立审计/证据/AUD-402-判读-BD1.csv"
HEADER = ["符号/键", "位置(路径:行)", "现行值", "单位", "坐标系/归一化", "精度要求",
          "有效有限域", "来源现状", "出处", "处置", "适用域", "备注"]

def emit(rows):
    new = not os.path.exists(OUT)
    with io.open(OUT, "a", newline="", encoding="utf-8-sig") as fh:
        w = csv.writer(fh, quoting=csv.QUOTE_MINIMAL, lineterminator="\n")
        if new:
            w.writerow(HEADER)
        for r in rows:
            assert len(r) == 12, (len(r), r[0])
            w.writerow(r)
    print("appended %d rows -> %s (now %d lines)" % (
        len(rows), OUT, sum(1 for _ in io.open(OUT, encoding="utf-8-sig"))))
