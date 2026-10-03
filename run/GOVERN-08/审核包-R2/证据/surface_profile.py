#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""产出面成分分析（GEN-2 判定式的输入事实）"""
import sys

SCRIPT_EXT = {".py", ".sh", ".cpp", ".c", ".h", ".hpp", ".cmake", ".m4",
              ".y", ".tex", ".md", ".rst", ".txt.log"}

SURFACES = [
    ("O1 实验/*/results",
     lambda p: p.startswith("实验/") and "/results/" in p),
    ("O2 实验/engineering-evidence",
     lambda p: p.startswith("实验/engineering-evidence/")),
    ("O3 eng/tests/validation/release02",
     lambda p: p.startswith("eng/tests/validation/release02/")),
]


def ext_of(p):
    base = p.rsplit("/", 1)[-1]
    return base[base.rfind("."):].lower() if "." in base else "(noext)"


def main():
    rows = []
    with open(sys.argv[1], encoding="utf-8") as f:
        next(f)
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            p, n, b, note = line.split("\t")
            rows.append((p, int(n)))

    for name, pred in SURFACES:
        hits = [(p, n) for p, n in rows if pred(p)]
        scr = [x for x in hits if ext_of(x[0]) in SCRIPT_EXT]
        dat = [x for x in hits if ext_of(x[0]) not in SCRIPT_EXT]
        print(f"{name}: total {len(hits)} 份 / {sum(n for _, n in hits)} 行"
              f"  |  脚本·文档 {len(scr)} 份 / {sum(n for _, n in scr)} 行"
              f"  |  数据 {len(dat)} 份 / {sum(n for _, n in dat)} 行")
        for p, n in sorted(scr, key=lambda x: -x[1])[:5]:
            print(f"      脚本/文档 {n:6d}  {p}")
        ext = {}
        for p, n in dat:
            e = ext_of(p)
            ext[e] = ext.get(e, 0) + 1
        print("      数据扩展名:", sorted(ext.items(), key=lambda x: -x[1])[:10])
        print()


if __name__ == "__main__":
    main()
