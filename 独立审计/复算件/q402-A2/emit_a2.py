"""AUD-402 A2 产出写入器/正规器。

1) normalize: 把已有行按 csv 规则重写（修复字段内 ASCII 逗号/引号造成的错列）。
   对 >12 字段的行，把尾部多余字段并回最后一列（备注）——仅在错位发生于备注时安全，
   由 check_a2.py 用 (符号|位置) 与队列逐行对齐来兜底验证。
2) append: 从 @@ 分隔的分块文本追加行，强制每行恰 12 字段。

用法:
  python emit_a2.py normalize
  python emit_a2.py append <partfile>
"""
import csv
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "raw", "AUD-402-判读-A2.csv"))
HDR = ["符号/键", "位置(路径:行)", "现行值", "单位", "坐标系/归一化", "精度要求",
       "有效有限域", "来源现状", "出处", "处置", "适用域", "备注"]


def read_rows(path):
    return list(csv.reader(io.open(path, encoding="utf-8", newline="")))


def normalize():
    rows = read_rows(OUT)
    fixed = []
    for i, r in enumerate(rows):
        if i == 0:
            fixed.append(HDR)
            continue
        if len(r) > 12:
            r = r[:11] + [",".join(r[11:])]
            print(f"  merge line {i+1} -> 12 fields ({r[1]})")
        if len(r) != 12:
            raise SystemExit(f"line {i+1}: {len(r)} fields, cannot fix: {r[:2]}")
        fixed.append(r)
    with io.open(OUT, "w", encoding="utf-8", newline="") as f:
        csv.writer(f, quoting=csv.QUOTE_MINIMAL).writerows(fixed)
    print(f"normalize ok: {len(fixed)-1} data rows")


def append(partfile):
    lines = [x for x in io.open(partfile, encoding="utf-8").read().splitlines() if x.strip()]
    add = []
    for lineno, line in enumerate(lines, 1):
        f = line.split("@@")
        if len(f) != 12:
            raise SystemExit(f"{partfile} line {lineno}: {len(f)} fields (need 12): {f[0][:40]}")
        add.append([x.strip() for x in f])
    with io.open(OUT, "a", encoding="utf-8", newline="") as f:
        csv.writer(f, quoting=csv.QUOTE_MINIMAL).writerows(add)
    print(f"append +{len(add)} -> total {len(read_rows(OUT))-1} data rows")


if __name__ == "__main__":
    if sys.argv[1] == "normalize":
        normalize()
    else:
        append(sys.argv[2])
