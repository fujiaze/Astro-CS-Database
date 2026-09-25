"""AUD-402 A2 产出自检：列数/表头一致、位置列与输入队列逐行对齐。

用法: python check_a2.py <判读csv> <队列csv> [<队列csv> ...]
只读，不写。
"""
import csv
import sys

COLS = 12
HDR = ["符号/键", "位置(路径:行)", "现行值", "单位", "坐标系/归一化", "精度要求",
       "有效有限域", "来源现状", "出处", "处置", "适用域", "备注"]


def rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.reader(f))


def main():
    out, queues = sys.argv[1], sys.argv[2:]
    data = rows(out)
    bad = 0
    if data[0] != HDR:
        print("HEADER-MISMATCH:", data[0])
        bad += 1
    body = data[1:]
    for i, r in enumerate(body, 2):
        if len(r) != COLS:
            print(f"COLCOUNT line {i}: {len(r)} -> {r[:2]}")
            bad += 1
        elif not all(c.strip() for c in (r[0], r[1], r[7], r[9])):
            print(f"EMPTY-REQUIRED line {i}: {r[:2]}")
            bad += 1
    want = []
    for q in queues:
        for r in rows(q)[1:]:
            want.append((r[0], r[1], r[2]))
    got = [(r[0], r[1], r[2]) for r in body]
    print(f"rows out={len(got)} queue={len(want)}")
    if len(got) != len(want):
        print(f"COUNT-DIFF: {len(want) - len(got)} missing")
    ws = {f"{a}|{b}" for a, b, _ in want}
    gs = {f"{a}|{b}" for a, b, _ in got}
    for k in sorted(ws - gs):
        print("  MISSING-ROW", k)
        bad += 1
    for k in sorted(gs - ws):
        print("  EXTRA-ROW", k)
        bad += 1
    dup = len(got) - len({(a, b) for a, b, _ in got})
    if dup:
        print(f"DUPLICATE-ROWS {dup}")
        bad += 1
    print("RESULT:", "OK" if bad == 0 else f"{bad} problem(s)")


if __name__ == "__main__":
    main()
