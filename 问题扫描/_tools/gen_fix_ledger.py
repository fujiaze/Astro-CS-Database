# -*- coding: utf-8 -*-
"""修复账本生成器 gen_fix_ledger.py

从 问题扫描/findings/** 抽取全部条目，生成机器可读写账本：
  账本/FIX_LEDGER.csv   ← 隔壁在这里填处置列（判定列只读）
  账本/FIX_LEDGER.jsonl  ← 同数据的逐行 JSON，便于程序消费
  账本/FIX_LEDGER.md    ← 人读视图（P0 优先）
重跑本脚本时**保留隔壁已填的处置列**（按 id 合并），只刷新判定列。
"""""
import os, re, csv, json, sys, hashlib
ROOT = "问题扫描"
FIND = os.path.join(ROOT, "findings")
BOOK = os.path.join(ROOT, "账本")
os.makedirs(BOOK, exist_ok=True)
ID_RE = re.compile(r"^#{2,4}\s+((?:M\w+|L\d+b?c?d?e?|FD|V\d+|W\d+|SA)-[A-Z]{1,4}\d*-\d+)\b(.*)$")
KEY = [("category", ["- 类别", "**类别**", "类别:"]),
       ("priority", ["- 建议优先级", "**优先级**", "优先级:"]),
       ("position", ["- 位置", "**位置**", "位置:"]),
       ("evidence", ["- 证据摘录", "**证据", "- 取证口径"]),
       ("impact", ["- 影响", "**影响**"]),
       ("related", ["- related", "**related**", "- **关联"]),
       ("clause", ["- 权威依据", "**依据条款**"]),
       ("disp", ["- 建议处置", "**建议处置**", "- 处置"]),
       ("state", ["- 状态", "**状态**"]),
       ("conf", ["- 置信度", "**置信度**"])]
PRI = re.compile(r"(P0|P1|P2)")
def cat_of(path):
    parts = path.split(os.sep)
    return parts[-3] if len(parts) >= 3 else "?"
def blockv(b, i):
    ls = b[i:].split("\n")
    head = re.sub(r"^[-*\s]*[^:：]*[:：]\s*", "", ls[0]).strip("* ")
    if head: return head
    acc = []
    for ln in ls[1:]:
        if re.match(r"^- ", ln) or ln.startswith("#") or ln.startswith(">"): break
        if not ln.strip(): break
        acc.append(re.sub(r"^[-*\s]+", "", ln.strip()))
    return " ；".join(acc)

def rows_from(fp):
    with open(fp, encoding="utf-8", errors="replace") as f:
        lines = f.read().split("\n")
    out, cur = [], None
    for ln in lines:
        m = ID_RE.match(ln)
        if m:
            if cur: out.append(cur)
            cur = {"id": m.group(1), "title": m.group(2).strip(), "body": []}
        elif cur is not None:
            if re.match(r"^#{1,4}\s", ln) and not ID_RE.match(ln):
                pass
            cur["body"].append(ln)
    if cur: out.append(cur)
    res = []
    for e in out:
        b = "\n".join(e["body"])
        d = {"id": e["id"], "title": re.sub(r"\s*[-—|]\s*$", "", e["title"])[:220], "file": fp.replace(os.sep, "/"), "category": cat_of(fp)}
        for k, pats in KEY:
            for p in pats:
                i = b.find(p)
                if i >= 0:
                    seg = b[i:b.find("\n", i)] if k in ("priority","category","conf") else blockv(b, i)
                    seg = re.sub(r"^[-*\s]*[^:：]*[:：]\s*", "", seg).strip("* ")
                    if k == "evidence":
                        j = b.find("- 权威依据", i); k2 = b[i:i+400] if j < 0 else b[i:min(j, i+400)]
                        seg = " / ".join(x.strip()[:140] for x in re.findall(r">\s*(.+)", k2))[:300] or seg[:200]
                    d[k] = seg[:520]
                    break
        d["category"] = re.split(r"\s*[｜|]\s*", d.get("category",""))[0].strip("* ：:")
        if not re.match(r"^[A-Z_]+$", d["category"] or ""): d["category"] = cat_of(fp)
        d.setdefault("category", cat_of(fp))
        mp = PRI.search(d.get("priority", "")) or PRI.search(fp.upper())
        d["priority"] = (mp.group(1) if mp else "P?").upper()
        d["producer"] = re.match(r"([A-Za-z0-9]+?)_", os.path.basename(fp)).group(1) if re.match(r"([A-Za-z0-9]+?)_", os.path.basename(fp)) else os.path.basename(fp)[:-3]
        res.append(d)
    return res
rows = []
for dp, dn, fns in os.walk(FIND):
    for fn in sorted(fns):
        if fn.endswith(".md") and fn != "README.md":
            rows.extend(rows_from(os.path.join(dp, fn)))
seen, uniq = set(), []
for r in rows:
    if r["id"] in seen: continue
    seen.add(r["id"]); uniq.append(r)
dec = {}
dp = os.path.join(ROOT, "40_OWNER_DECISIONS.md")
if os.path.exists(dp):
    dlines = open(dp, encoding="utf-8", errors="replace").read().split("\n")
    cur = ""
    for ln in dlines:
        m = re.search(r"\|\s*(?:~~)?([ABC]-\d+)(?:~~)?\s*\|", ln)
        if m: cur = m.group(1)
        for rid in seen:
            if rid in ln and cur: dec.setdefault(rid, cur)
FIXCOLS = ["fix_state","fix_commit","fix_date","regression_test","fixed_by","fix_note","verified_state","verified_by","verified_date","verified_note"]
JUDG = ["id","priority","category","producer","release_blocker","owner_decision","title","position","evidence","impact","clause","related","suggested_disposition","evidence_file","source_line_state"]
prev = {}
old = os.path.join(BOOK, "FIX_LEDGER.csv")
if os.path.exists(old):
    for r in csv.DictReader(open(old, encoding="utf-8-sig")):
        prev[r["id"]] = {c: r.get(c, "") for c in FIXCOLS}
def blocker(r):
    if r["priority"] == "P0": return "Y"
    if r["priority"] == "P1": return "Y"
    return "N"
out = []
for r in sorted(uniq, key=lambda x: (x["priority"], x["producer"], x["id"])):
    cat = re.split(r"\s*[｜|]\s*", r.get("category",""))[0].strip("* ：:")
    if not re.match(r"^[A-Z_]{3,}$", cat): cat = r["category"]
    row = {"id": r["id"], "priority": r["priority"], "category": cat, "producer": r["producer"],
           "release_blocker": blocker(r), "owner_decision": dec.get(r["id"], ""), "title": r.get("title",""),
           "position": r.get("position",""), "evidence": r.get("evidence",""), "impact": r.get("impact",""),
           "clause": r.get("clause",""), "related": r.get("related",""), "suggested_disposition": r.get("disp",""),
           "evidence_file": r["file"], "source_line_state": r.get("state","")}
    for c in FIXCOLS:
        row[c] = prev.get(r["id"], {}).get(c, "") or ("OPEN" if c == "fix_state" else "")
    out.append(row)
cols = JUDG + FIXCOLS
with open(os.path.join(BOOK, "FIX_LEDGER.csv"), "w", newline="", encoding="utf-8-sig") as f:
    w = csv.DictWriter(f, cols); w.writeheader()
    for r in out: w.writerow(r)
with open(os.path.join(BOOK, "FIX_LEDGER.jsonl"), "w", encoding="utf-8") as f:
    for r in out: f.write(json.dumps(r, ensure_ascii=False) + "\n")
cnt = {}
for r in out: cnt[r["priority"]] = cnt.get(r["priority"], 0) + 1
p0 = [r for r in out if r["priority"] == "P0"]
M = ["# FIX_LEDGER · 修复账本（人读视图：P0 全列）", "",
     "- 条目总数 **%d**（P0 %s / P1 %s / P2 %s / 其它 %s）；机器读写面 = " % (len(out), cnt.get('P0',0), cnt.get('P1',0), cnt.get('P2',0), cnt.get('P?',0)),
     "  `问题扫描/账本/FIX_LEDGER.csv`（**隔壁只填后 9 列**，判定列由前台重跑刷新并按 id 保留你的填写）",
     "- 重跑：`python3 问题扫描/_tools/gen_fix_ledger.py`", "",
     "| id | P | 类别 | 产出方 | 裁决 | 标题（截断） | fix_state | fix_commit | 回归锁 |",
     "|---|---|---|---|---|---|---|---|---|"]
for r in p0:
    M.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (r["id"], r["priority"], r["category"], r["producer"], r["owner_decision"], r["title"][:70].replace("|","/") or r["id"], r["fix_state"], r["fix_commit"], r["regression_test"]))
open(os.path.join(BOOK, "FIX_LEDGER.md"), "w", encoding="utf-8").write("\n".join(M) + "\n")
print("条目=%d  P0=%s  P1=%s  P2=%s  已填处置=%d  文件=%s" % (
    len(out), cnt.get("P0",0), cnt.get("P1",0), cnt.get("P2",0),
    sum(1 for r in out if r["fix_commit"] or r["fix_state"] not in ("OPEN","")), BOOK))