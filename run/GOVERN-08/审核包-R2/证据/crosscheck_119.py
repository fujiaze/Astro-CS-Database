#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GOVERN-08 R2 · 交叉核对：派单给的「119 份巨型生成 JSON / 925,309 行 / 占 52.9%」
能否在本仓 HEAD f9650dd0 复现？GEN-2 判定式是否把这 119 份全部归入生成物？
"""
import sys


def main():
    rows = []
    with open(sys.argv[1], encoding="utf-8") as f:
        next(f)
        for line in f:
            line = line.rstrip("\n")
            if line:
                rows.append(line.split("\t"))

    tot_l = sum(int(r[1]) for r in rows)
    jsonish = [(r[0], int(r[1])) for r in rows
               if r[0].endswith((".json", ".jsonl"))]
    jsononly = [(r[0], int(r[1])) for r in rows if r[0].endswith(".json")]

    print("=== 1. 派单数字的复现性检验（HEAD f9650dd0）===")
    print(f"主体总行数              = {tot_l:,}")
    print(f"主体总份数              = {len(rows):,}")
    print(f"全部 .json             = {len(jsononly):,} 份 / "
          f"{sum(n for _, n in jsononly):,} 行")
    print(f"全部 .json+.jsonl      = {len(jsonish):,} 份 / "
          f"{sum(n for _, n in jsonish):,} 行")
    srt = sorted(jsononly, key=lambda x: -x[1])
    print(f"行数最大的 119 份 .json  = {sum(n for _, n in srt[:119]):,} 行")
    print(f"行数最大的 200 份 .json  = {sum(n for _, n in srt[:200]):,} 行")
    print()
    print("派单称：119 份 = 925,309 行 = 主体 52.9%")
    print(f"实测：119 份 = {sum(n for _, n in srt[:119]):,} 行 "
          f"= 主体 {sum(n for _, n in srt[:119]) / tot_l * 100:.1f}%")
    print(f"实测：全部 590 份 .json = {sum(n for _, n in jsononly):,} 行 "
          f"= 主体 {sum(n for _, n in jsononly) / tot_l * 100:.1f}%")
    print("→ 结论：925,309 > 主体全部 .json 的行数总和，")
    print("  故「119 份 JSON = 925,309 行」在 HEAD 上**不可复现**，任何取 119 份 JSON 的口径都对不上。")
    print()

    print("=== 2. GEN-2 判定式对这 119 份的覆盖 ===")
    verdict = {}
    for r in rows:
        verdict[r[0]] = (int(r[3]), r[5])
    top119 = srt[:119]
    gen = [x for x in top119 if verdict[x[0]][0] == 1]
    notgen = [x for x in top119 if verdict[x[0]][0] == 0]
    print(f"119 份中被 GEN-2 判为生成物 : {len(gen):3d} 份 / "
          f"{sum(n for _, n in gen):,} 行")
    print(f"119 份中未被判为生成物     : {len(notgen):3d} 份 / "
          f"{sum(n for _, n in notgen):,} 行")
    for p, n in notgen:
        print(f"    漏网 {n:7d}  {p}   [信号: {verdict[p][1]}]")
    print()

    print("=== 3. GEN-2 生成物总量（真实口径）===")
    allgen = [(r[0], int(r[1])) for r in rows if int(r[3]) == 1]
    print(f"GEN-2 生成物 = {len(allgen)} 份 / {sum(n for _, n in allgen):,} 行"
          f" = 主体 {sum(n for _, n in allgen) / tot_l * 100:.1f}%")
    print("→ 派单的 119 份是「按体量猜的集合」，不是判定式的输出；")
    print("  判定式的输出是 484 份 / 657,306 行 / 37.6%（详见 判定摘要.txt）。")


if __name__ == "__main__":
    main()
