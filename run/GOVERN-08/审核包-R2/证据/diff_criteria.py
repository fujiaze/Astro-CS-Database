#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GOVERN-08 R2 · 双车道判定式差异比对
==================================
只读。把两条并行车道（对方 G1 vs 本单 GEN-2）的逐份判定并排 diff，
逐条列出分歧文件，供前台裁决「取哪套判定式 / 差异是误报还是漏报」。

用法: python3 diff_criteria.py <对方逐份判定.csv> <本单逐份判定.tsv>
"""
import csv
import sys
import collections

MINE_GEN, MINE_TP, MINE_ART = "生成物", "第三方", "人工产物"
THEIRS = {"GENERATED": "生成物", "VENDOR": "第三方", "HUMAN": "人工产物"}


def load_theirs(path):
    d = {}
    with open(path, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            d[r["path"]] = (THEIRS.get(r["tier"], r["tier"]), int(r["lines"]),
                            r.get("layer", ""), r.get("reason", ""))
    return d


def load_mine(path):
    d = {}
    with open(path, encoding="utf-8") as f:
        next(f)
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            p, n, b, g, t, sig, basis, exc = line.split("\t")
            tier = MINE_GEN if g == "1" else (MINE_TP if t == "1" else MINE_ART)
            d[p] = (tier, int(n), sig)
    return d


def main():
    theirs = load_theirs(sys.argv[1])
    mine = load_mine(sys.argv[2])

    only_theirs = sorted(set(theirs) - set(mine))
    only_mine = sorted(set(mine) - set(theirs))
    A, B = set(theirs), set(mine)

    L = []
    o = L.append
    o("=== 双车道判定式差异比对 ===")
    o(f"对方逐份判定 {len(theirs)} 份   本单逐份判定 {len(mine)} 份")
    o(f"仅对方有 {len(only_theirs)} 份   仅本单有 {len(only_mine)} 份")
    for p in only_theirs[:20]:
        o(f"    仅对方 {theirs[p][0]:6s} {theirs[p][1]:7d}  {p}")
    for p in only_mine[:20]:
        o(f"    仅本单 {mine[p][0]:6s} {mine[p][1]:7d}  {p}")
    o("")

    diff = []
    for p in sorted(A & B):
        if theirs[p][0] != mine[p][0]:
            diff.append(p)
    o(f"--- 分级不一致 {len(diff)} 份 ---")
    mat = collections.Counter()
    matl = collections.Counter()
    for p in diff:
        k = (theirs[p][0], mine[p][0])
        mat[k] += 1
        matl[k] += theirs[p][1]
    for k, c in mat.most_common():
        o(f"  对方={k[0]:6s} → 本单={k[1]:6s}   {c:4d} 份 {matl[k]:8d} 行")
    o("")

    o("--- 逐份分歧明细（按行数降序）---")
    for p in sorted(diff, key=lambda x: -theirs[x][1]):
        o(f"  {theirs[p][1]:7d}  {p}")
        o(f"          对方: {theirs[p][0]:6s} [层 {theirs[p][2]}] {theirs[p][3][:90]}")
        o(f"          本单: {mine[p][0]:6s} [信号 {mine[p][2]}]")
    o("")

    o("--- 分级矩阵（行数）---")
    tiers = ["人工产物", "生成物", "第三方"]
    o(f"{'':10s}" + "".join(f"{t:>12s}" for t in tiers) + f"{'合计':>12s}")
    for ta in tiers:
        row = f"{ta:10s}"
        tot = 0
        for tb in tiers:
            v = sum(theirs[p][1] for p in A & B
                    if theirs[p][0] == ta and mine[p][0] == tb)
            row += f"{v:12,d}"
            tot += v
        o(row + f"{tot:12,d}")

    out = "\n".join(L)
    with open(sys.argv[3] if len(sys.argv) > 3 else "/dev/stdout",
              "w", encoding="utf-8") as f:
        f.write(out + "\n")
    print(out)


if __name__ == "__main__":
    main()
