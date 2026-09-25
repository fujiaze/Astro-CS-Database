#!/usr/bin/env python3
"""Strict D1 coverage v2: entry counts only if its block carries >=9 of 13 field labels.

Field labels appear in three ledger shapes: vertical field table, wide positional table
(header row declares labels once), and bullet lists with synonym labels.
"""
import sys, os, io, re

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
BASE = r"产出/"
INV, RAW = os.path.join(BASE, "inventory"), os.path.join(BASE, "raw")
WORK = os.path.join(BASE, "复算", "d1gap3")

def rd(p):
    with open(p, encoding="utf-8", errors="replace") as f:
        return f.read()

def lst(name):
    return [x.strip().replace("\\", "/") for x in rd(os.path.join(INV, name)).splitlines() if x.strip()]

universe = lst("DA-all.txt")

BATCH = {
    "AUD-101-DA01-根规范与科学.md": ["DA-01.txt"],
    "AUD-101-DA02-算法推导.md": ["DA-02.txt"],
    "AUD-101-D1残余.md": ["DA-02-missing.txt", "D1-residual.txt"],
    "AUD-101-DB01.md": ["DB-01.txt"], "AUD-101-DB02.md": ["DB-02.txt"],
    "AUD-101-DB-03.md": ["DB-03.txt"], "AUD-101-DB-04.md": ["DB-04.txt"],
    "AUD-101-DB-05.md": ["DB-05.txt"], "AUD-101-DB-06-07.md": ["DB-06.txt", "DB-07.txt"],
    "AUD-101-DB-08.md": ["DB-08.txt"], "AUD-101-DB-09.md": ["DB-09.txt"],
    "AUD-101-DB-10.md": ["DB-10.txt"], "AUD-101-DB-10-补.md": ["DB-10.txt"],
    "AUD-101-DB-11.md": ["DB-11.txt"], "AUD-101-DB-12.md": ["DB-12.txt"],
    "AUD-101-DB-13.md": ["DB-13.txt"], "AUD-101-DB-14.md": ["DB-14.txt"],
    "AUD-101-DB-15.md": ["DB-15.txt"], "AUD-101-DB-16.md": ["DB-16.txt"],
    "AUD-101-DB-17.md": ["DB-17.txt"], "AUD-101-DB-18.md": ["DB-18.txt"],
    "AUD-101-DB-19.md": ["DB-19.txt"], "AUD-101-DB-20.md": ["DB-20.txt"],
}

FIELDS = {
    "标题": r"标题",
    "行数": r"行数",
    "角色": r"角色",
    "主题": r"主题",
    "上游": r"上游",
    "下游": r"下游",
    "是否正本": r"是否正本|正本性|是否唯一正本",
    "重复或重叠对象": r"重复[/一、]|重叠对象|重复或重叠",
    "元信息块": r"元信息",
    "写法违规": r"写法违规|日期·任务编号|违规",
    "悬空引用": r"悬空",
    "与上位冲突": r"与上位|上位冲突|与最高设计冲突",
    "处置建议": r"处置",
}
LBL = re.compile(r"^\s*(?:[-*·]\s*\**|[|\uFF5C]\s*\**)\s*(" +
                 "|".join(f"(?:{v})" for v in FIELDS.values()) + r")", re.M)

EXT = r"(?:md|py|yaml|yml|csv|json|txt)"
HEADING = re.compile(r"^#{2,6}[^\n]*?([^\s|`]+?\." + EXT + r")")
TBLROW = re.compile(r"^\s*\|\s*`?([^\s|`]+?\." + EXT + r")`?\s*\|")

def field_hits(block):
    return sum(1 for k, v in FIELDS.items() if re.search(
        r"(?:^|\n)\s*(?:[-*·]\s*|\|\s*)\**\s*(?:" + v + r")", block) or
        (v in block and re.search(r"[|\uFF5C]\s*" + v, block)))

strict, loose, detail = {}, set(), {}
for n in sorted(BATCH):
    lines = rd(os.path.join(RAW, n)).splitlines()
    own = list(dict.fromkeys(sum((lst(b) for b in BATCH[n]), [])))
    byname = {}
    for p in own:
        byname.setdefault(os.path.basename(p), []).append(p)
    wide = any(l.lstrip().startswith("|") and sum(1 for v in FIELDS.values() if re.search(v, l)) >= 9
               for l in lines)
    pos = []
    for i, l in enumerate(lines):
        m = HEADING.match(l)
        tok = m.group(1) if m else None
        if tok is None:
            t = TBLROW.match(l)
            tok = t.group(1) if t else None
        if tok:
            pos.append((i, tok))

    def resolve(tok):
        t = tok.replace("\\", "/").lstrip("./")
        hits = [p for p in own if p == t or p.endswith("/" + t)]
        if len(hits) == 1:
            return hits[0]
        if t in byname and len(byname[t]) == 1:
            return byname[t][0]
        return None

    per = {}
    for k, (i, tok) in enumerate(pos):
        end = pos[k + 1][0] if k + 1 < len(pos) else len(lines)
        p = resolve(tok)
        if p is None:
            continue
        loose.add(p)
        block = "\n".join(lines[i:end])
        fc = 13 if wide else len([1 for kk, vv in FIELDS.items() if re.search(vv, block)])
        if fc > per.get(p, 0):
            per[p] = fc
            detail[p] = (n, fc)
    nmiss = [p for p in own if p not in per]
    weak = {p: per.get(p, 0) for p in own if per.get(p, 0) < 9}
    for p, c in per.items():
        if c >= 9:
            strict.setdefault(p, []).append((n, c))
    print(f"{n}: 分配{len(own)} 宽表={wide} 无条目位{len(nmiss)} 弱(<9){len(weak)}" +
          ("  " + ", ".join(n2 for n2 in nmiss[:3]) if nmiss else ""))

gap = [p for p in universe if p not in strict]
print("\nSTRICT universe", len(universe), " covered", len(set(universe) & set(strict)), " GAP", len(gap))
for p in gap:
    print("   GAP:", p, "| loose:", p in loose, "| fields:", detail.get(p))
with open(os.path.join(WORK, "strict_gap.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(gap) + "\n")
print("\n--- three candidates ---")
for p in ["docs/standards/TEST_STANDARD.md",
          "docs/algorithms/anchors/check_doc_line_anchors.py",
          "docs/standards/checks/check_standards_registry.py"]:
    print("  ", p, "->", strict.get(p, "NONE"), "loose:", p in loose, "detail:", detail.get(p))
