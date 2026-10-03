#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GOVERN-08 R2 · 历史「轮1v3」族 14 份交付件 → 新片划分的覆盖重判
=================================================================
只读。逐份解析 轮1v3-片NN.md 的 §0 覆盖率表，还原它真正读了哪些**文件**，
再按新片划分（GEN-2-SLICE）重判覆盖。**不采信对方的中间数字，只用其原文。**

用法:
    python3 historical_coverage.py <轮1v3目录> <片清单.yaml> <输出CSV> <输出摘要>
"""
import re
import os
import sys
import glob
import csv
import collections

MAN_RE = re.compile(r'/tmp/r3/(s\d+)\.txt')
PATH_RE = re.compile(
    r'(?:docs|实验|lib|eng)/[^\s`|、，,）)】\]]*'
    r'\.(?:cpp|cc|c|h|hpp|py|md|json|jsonl|csv|sh|yaml|yml|txt|cmake|in|m4)')
INT_RE = re.compile(r'(\d[\d,]*)')
# §0 里「非本片、只作旁证」的交叉打开清单的起始标记 —— 这些**不计入覆盖率**
CROSSREF_MARKERS = [
    "额外交叉打开", "被引用文件", "旁证", "为核", "为判定语义另开",
    "为确认发现而额外打开", "排除项", "超出本片", "非本片",
]


def section0(txt):
    """取 §0 小节正文：'## §0' 到下一个 '## ' 标题。"""
    i = txt.find("## §0")
    if i < 0:
        return ""
    j = txt.find("\n## ", i + 1)
    return txt[i:j if j > 0 else len(txt)]


def first_table(block):
    """取 §0 里的第一张 markdown 表（连续以 | 开头的行），到 合计 行或表尾为止。"""
    rows, started = [], False
    for line in block.splitlines():
        s = line.strip()
        if s.startswith("|"):
            started = True
            if re.match(r'^\|[\s:\-|]+\|$', s):      # 分隔行
                continue
            if "合计" in s or "小计" in s:
                break
            rows.append(s)
        elif started and rows:
            break                                     # 表结束
    return rows


def parse_row_cells(row):
    cells = [c.strip() for c in row.strip().strip("|").split("|")]
    for idx, c in enumerate(cells):
        m = PATH_RE.search(c)
        if not m:
            continue
        nums = []
        for j in range(idx, len(cells)):
            for t in INT_RE.findall(cells[j]):
                nums.append(int(t.replace(",", "")))
        nums = [x for x in nums if 0 < x < 10 ** 7]
        yield m.group(0), (nums[0] if nums else 0)


def parse_deliverable(path):
    """返回 (manifest, [(file, lines)], 自述合计行数)"""
    txt = open(path, encoding="utf-8", errors="replace").read()
    block = section0(txt)
    # 截断到「旁证/交叉打开」之前，避免把非本片文件算进覆盖率
    cut = len(block)
    for mk in CROSSREF_MARKERS:
        k = block.find(mk)
        if 0 <= k < cut:
            cut = k
    block = block[:cut]

    man = None
    m = MAN_RE.search(block) or MAN_RE.search(txt)
    if m:
        man = m.group(1)

    rows = first_table(block)
    files, seen = [], set()
    for r in rows:
        for f, n in parse_row_cells(r):
            if f not in seen:
                seen.add(f)
                files.append((f, n))
    declared = 0
    for line in txt.splitlines():
        mm = re.search(r'实读\s*([\d,]+)\s*/\s*([\d,]+)\s*行', line) or \
             re.search(r'\*\*合计\*\*\s*\|\s*\*\*([\d,]+)\*\*', line)
        if mm:
            declared = int(mm.group(1).replace(",", ""))
            break
    return man, files, declared


def main():
    src_dir, yaml_path, csv_out, sum_out = sys.argv[1:5]

    import yaml
    doc = yaml.safe_load(open(yaml_path, encoding="utf-8"))
    file2slice = {}
    slice_lines = {}
    slice_dom = {}
    for s in doc["片"]:
        slice_lines[s["片号"]] = s["实际行数"]
        slice_dom[s["片号"]] = s["分层键"]
        for m in s["成员文件"]:
            file2slice[m["路径"]] = s["片号"]

    files = sorted(glob.glob(os.path.join(src_dir, "轮1v3-片*.md")))
    if not files:
        files = sorted(glob.glob(os.path.join(src_dir, "轮*.md")))

    per_deliv = []
    for p in files:
        man, rows, declared = parse_deliverable(p)
        per_deliv.append((os.path.basename(p), man, rows, declared))

    # ---- 旧片 → 新片映射 ----
    oldman_files = collections.defaultdict(set)
    oldman_delivs = collections.defaultdict(list)
    for name, man, rows, declared in per_deliv:
        key = man or "(未声明)"
        for f, n in rows:
            oldman_files[key].add(f)
        oldman_delivs[key].append(name)

    # ---- 新片覆盖（按文件集合去重）----
    covered_files = set()
    per_new_slice = collections.defaultdict(set)
    for f in set().union(*oldman_files.values()) if oldman_files else set():
        s = file2slice.get(f)
        if s:
            covered_files.add(f)
            per_new_slice[s].add(f)

    # 行数按当前 HEAD 重取（交付件记的是当时行数，HEAD 已漂）
    import subprocess
    head_lines = {}
    with open(os.path.join(os.path.dirname(yaml_path), "..", "证据",
                           "主体行数.tsv"), encoding="utf-8") as fh:
        next(fh)
        for line in fh:
            line = line.rstrip("\n")
            if line:
                pp, nn, bb, nt = line.split("\t")
                head_lines[pp] = int(nn)

    covered_lines = sum(head_lines.get(f, 0) for f in covered_files)
    tot_files = len(file2slice)
    tot_lines = sum(slice_lines.values())

    # ---- 输出 CSV ----
    with open(csv_out, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["交付件", "旧清单", "旧清单份数", "声明份数", "解析文件数",
                    "新片", "新片行数", "该片被读文件数"])
        for name, man, rows, declared in sorted(per_deliv):
            news = sorted({file2slice[f] for f, _ in rows if f in file2slice})
            w.writerow([name, man, len({f for f, _ in rows}), declared,
                        len(rows), ";".join(news),
                        sum(slice_lines[s] for s in news),
                        len({f for f, _ in rows if f in file2slice})])

    # ---- 摘要 ----
    L = []
    o = L.append
    o("=== 历史「轮1v3」族覆盖重判（按本单 GEN-2-SLICE 新片划分）===")
    o(f"新片划分：{len(slice_lines)} 片 / {tot_files} 份 / {tot_lines} 行")
    o("")
    o(f"--- 交付件 → 旧清单（/tmp/r3/*.txt 已空，旧片划分不可复现，只能从 §0 表还原）---")
    for name, man, rows, declared in sorted(per_deliv):
        o(f"  {name:22s} 旧清单={man or '(未声明)':8s} "
          f"解析文件 {len(rows):2d} 份（自陈 {declared}）")
    o("")
    o("--- 旧清单 → 交付件（重复检测）---")
    for key in sorted(oldman_delivs):
        dl = oldman_delivs[key]
        tag = "  ← 重复覆盖" if len(dl) > 1 else ""
        o(f"  {key:10s} {len(dl)} 份交付件: {', '.join(sorted(dl))}"
          f"  文件 {len(oldman_files[key])} 份{tag}")
    distinct = len([k for k in oldman_delivs if k != "(未声明)"])
    dup = sum(len(v) - 1 for k, v in oldman_delivs.items() if len(v) > 1)
    o(f"  ⇒ {len(per_deliv)} 份交付件 = {distinct} 个不同旧清单 + {dup} 份重复")
    o("")
    o("--- 新片覆盖（去重后）---")
    o(f"  覆盖新片: {len(per_new_slice)} / {len(slice_lines)}"
      f"  ({len(per_new_slice) / len(slice_lines) * 100:.1f}%)")
    o(f"  覆盖份数: {len(covered_files)} / {tot_files}"
      f"  ({len(covered_files) / tot_files * 100:.1f}%)")
    o(f"  覆盖行数: {covered_lines} / {tot_lines}"
      f"  ({covered_lines / tot_lines * 100:.2f}%)   [行数按 HEAD f9650dd0 重取]")
    o("")
    o("--- 被覆盖的新片明细 ---")
    for s in sorted(per_new_slice, key=lambda x: -slice_lines[x]):
        fs = per_new_slice[s]
        o(f"  {s:8s} {slice_dom[s]:6s} 片行数 {slice_lines[s]:6d}  "
          f"被读 {len(fs):3d} 份  ({len(fs) / tot_files * 100:.2f}% 全域)")
    o("")
    o("--- 旧清单内不在新分母的文件（被 GEN-2 判为生成物/第三方）---")
    miss = collections.Counter()
    for key, fset in oldman_files.items():
        for f in fset:
            if f not in file2slice:
                miss[f] = head_lines.get(f, 0)
    for f, n in miss.most_common():
        o(f"  {n:7d}  {f}")
    o(f"  合计 {len(miss)} 份 / {sum(miss.values())} 行 —— 这些行不计入新分母，"
      f"即旧台账的「已读行数」有一部分落在被排除的面上。")

    out = "\n".join(L)
    with open(sum_out, "w", encoding="utf-8") as f:
        f.write(out + "\n")
    print(out)


if __name__ == "__main__":
    main()
