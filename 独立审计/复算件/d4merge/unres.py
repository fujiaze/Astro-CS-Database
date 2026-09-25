# -*- coding: utf-8 -*-
"""从四路判读成稿里抽取「待确认项的保守方向与影响面」表（UNRESOLVED 输入），机械转录不新增判读。"""
import io, os, re, csv

BASE = r"产出/"
RAW = os.path.join(BASE, "raw")

SPEC = [
    ("AUD-402-判读-A1.md", "3", "AUD-402-判读-A1 §3"),
    ("AUD-402-判读-A2.md", "4", "AUD-402-判读-A2 §4"),
    ("AUD-402-判读-A3.md", "3", "AUD-402-判读-A3 §3"),
    ("AUD-402-判读-BD1.md", "2", "AUD-402-判读-BD1 §2"),
]

out = io.open(os.path.join(BASE, "复算/d4merge/unresolved_items.csv"), "w",
              encoding="utf-8-sig", newline="")
wr = csv.writer(out)
wr.writerow(["来源成稿", "编号", "项", "保守方向／修法", "影响范围", "登记面"])

n = 0
for f, anchor, srcdoc in SPEC:
    txt = io.open(os.path.join(RAW, f), encoding="utf-8", errors="replace").read()
    lines = txt.split("\n")
    # 定位该节
    st = None
    want = re.compile(r"^#{2,3}\s*" + re.escape(anchor) + r"[\s.、].*待确认")
    for i, l in enumerate(lines):
        if want.match(l.strip()):
            st = i
            break
    if st is None:
        wr.writerow([srcdoc, "（未定位）", "", "", "", ""])
        continue
    en = st + 1
    while en < len(lines) and not (lines[en].startswith("## ") and en > st + 1):
        en += 1
    seg = lines[st:en]
    # 抽 markdown 表行
    for l in seg:
        s = l.strip()
        if not s.startswith("|"):
            continue
        cells = [c.strip() for c in s.strip("|").split("|")]
        if not cells or set("".join(cells)) <= set("-: "):
            continue
        if cells[0] in ("#", "位置:行", "条", "项") or "保守" in cells[1] or "待确认内容" in "".join(cells):
            continue
        if srcdoc.endswith("BD1 §2"):
            continue  # BD1 用编号列表，单独处理
        if len(cells) >= 4:
            wr.writerow([srcdoc, cells[0], cells[1], cells[2], cells[3], "待确认"])
            n += 1

# BD1 §2 是编号列表：1. **项** — 影响面：… 保守方向：…
txt = io.open(os.path.join(RAW, "AUD-402-判读-BD1.md"), encoding="utf-8", errors="replace").read()
m = re.search(r"## 2 待确认与影响范围.*?\n(.*?)\n---", txt, re.S)
if m:
    for l in m.group(1).split("\n"):
        s = l.strip()
        mm = re.match(r"^(\d+)\.\s+\*\*(.+?)\*\*\s*[—-]\s*(.+)$", s)
        if not mm:
            continue
        body = mm.group(3)
        cons = re.search(r"保守方向[：:](.+?)(?:[；;]|$)", body)
        imp = re.search(r"影响面[：:](.+?)[；;]", body)
        wr.writerow(["AUD-402-判读-BD1 §2", mm.group(1), mm.group(2),
                     cons.group(1).strip() if cons else body[:200],
                     imp.group(1).strip() if imp else "", "待确认"])
        n += 1

out.close()
print("extracted", n)
