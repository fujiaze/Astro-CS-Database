#!/usr/bin/env python3
"""AUD-101 D1 merge - stage 3: build the merged ledger + derived tables.

Reads   : records.json / instances.json (stages 1-2), the 360-doc universe,
          the repo's pinned HEAD tree (git show / git ls-files, read-only).
Writes  : merged.json, conflicts.json, gaps.json, derived.json
          ../../整改文档包/01_文档/文档现状审计台账.csv      (main table, 60-row appends)
          ../../整改文档包/01_文档/文档现状审计台账.md       (body + derived tables)
Deterministic: sorted iteration everywhere, no timestamps, no randomness.
"""
import io
import os
import re
import csv
import sys
import json
import subprocess
import collections

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

WORK = r"F:\Astro dev\独立审查\工包-AUDIT-06\产出\复算\d1merge"
INV = r"F:\Astro dev\独立审查\工包-AUDIT-06\产出\inventory"
OUT_DIR = r"F:\Astro dev\独立审查\工包-AUDIT-06\产出\整改文档包\01_文档"
REPO = r"F:\Astro dev\Astro CS Normalization Database"

FIELDS = ["路径", "标题", "行数", "角色", "主题", "上游", "下游", "是否正本",
          "重复或重叠对象", "元信息块", "写法违规", "悬空引用", "与上位冲突", "处置建议"]
JUDGE = ["角色", "是否正本", "处置建议"]
DETAIL_FORMS = {"V": 0, "B": 0, "Hc": 1, "H": 2}   # lower = more detailed view of the same batch
BATCH_ORDER = ["DA-01", "DA-02", "DB-01", "DB-02", "DB-03", "DB-04", "DB-05", "DB-06-07", "DB-07",
               "DB-08", "DB-09", "DB-10", "DB-10-补", "DB-11", "DB-12", "DB-13", "DB-14", "DB-15",
               "DB-16", "DB-17", "DB-18", "DB-19", "DB-20", "D1残余", "D1补三份"]


def bkey(b):
    return BATCH_ORDER.index(b) if b in BATCH_ORDER else 99


# ---------------------------------------------------------------- repo facts
def git(*a):
    return subprocess.run(["git", "-C", REPO, "-c", "core.quotePath=false"] + list(a),
                          capture_output=True, text=True, encoding="utf-8", errors="replace").stdout


def tracked_set():
    return {l.strip() for l in git("ls-files").splitlines() if l.strip()}


def index_paths():
    txt = git("show", "HEAD:docs/DOCUMENT_INDEX.yaml")
    return [m.group(1) for m in re.finditer(r'^\s*- path:\s*"?([^"\n]+?)"?\s*$', txt, re.M)]


# ---------------------------------------------------------------- merging
def merge_rows(inst, col_none=None):
    col_none = col_none or set()
    per = collections.defaultdict(list)
    for i in inst:
        per[i["path"]].append(i)
    rows = []
    conflicts = []
    for path in sorted(per):
        ins = sorted(per[path], key=lambda x: (bkey(x["batch"]), DETAIL_FORMS.get(x["form"], 3), x["entry"]))
        row = {"路径": path, "备注": [], "来源成稿": [], "来源条目号": [], "_sides": {}, "_未登记": []}
        seen_src = []
        for i in ins:
            tag = "%s:%s" % (i["batch"], re.sub(r"\s+", "", i["entry"])[:28])
            if i["batch"] not in seen_src:
                seen_src.append(i["batch"])
            row["来源条目号"].append(tag)
            if i["norm"] != i["path"] and i["norm"]:
                row["备注"].append("登载原样「%s」→ 归一「%s」（%s）" % (i["raw"][:70], i["path"], i["how"]))
        row["来源成稿"] = sorted(seen_src, key=bkey)
        for f in FIELDS[1:]:
            vals = []
            for i in ins:
                v = (i["fields"].get(f) or "").strip()
                if v:
                    vals.append((i["batch"], i["form"], v))
            uniq = []
            for b, fm, v in vals:
                key = re.sub(r"[\s　]+", "", v)
                if key and key not in {re.sub(r"[\s　]+", "", u[2]) for u in uniq}:
                    uniq.append((b, fm, v))
            if not uniq:
                src = row["来源成稿"]
                if all((b, f) in col_none for b in src):
                    row[f] = "（未出该列：%s 该列整列为空）" % "、".join(src)
                elif any((b, f) in col_none for b in src):
                    row[f] = "（未登记：%s 出了该列、本份留空）" % "、".join(
                        b for b in src if (b, f) in col_none)
                else:
                    row[f] = "（未登记：来源成稿该份留空）"
                row["_未登记"].append(f)
                continue
            row["_sides"][f] = uniq
            if len(uniq) == 1:
                row[f] = uniq[0][2]
                continue
            # disagreement: detail view of the same batch wins as primary; other batch wins并列
            primary = min(uniq, key=lambda u: (bkey(u[0]), DETAIL_FORMS.get(u[1], 3)))
            kept = [primary]
            for u in uniq:
                if u == primary:
                    continue
                su = re.sub(r"[\s　]+", "", u[2])
                sp = re.sub(r"[\s　]+", "", primary[2])
                if u[0] == primary[0] and su and (su in sp or sp in su):
                    continue        # 同成稿一览式与明细式互为包含：取明细，不重复并列
                kept.append(u)
            row["_sides"][f] = kept
            row[f] = primary[2]
            for b, fm, v in kept:
                if (b, fm, v) == primary:
                    continue
                same_batch = primary[0] == b
                conflicts.append({
                    "path": path, "field": f,
                    "type": "同稿双式" if same_batch else "批间不一致",
                    "a_batch": primary[0], "a_form": primary[1], "a": primary[2],
                    "b_batch": b, "b_form": fm, "b": v})
        rows.append(row)
    return rows, conflicts


# ---------------------------------------------------------------- theme groups
PATHISH = re.compile(r"[A-Za-z0-9_\-\u4e00-\u9fff][A-Za-z0-9_\-\u4e00-\u9fff./]*"
                     r"\.(?:md|ya?ml|json|csv|py|txt|tsv)")


def resolve_token(tok, uset, by_suffix):
    t = tok.strip().strip("`").replace("\\", "/")
    t = re.sub(r"^\.?/", "", t)
    t = re.sub(r"[:：]\d[\d,\-–~ ]*$", "", t)
    if t in uset:
        return t
    cand = by_suffix.get("/" + t)
    if cand and len(cand) == 1:
        return cand[0]
    base = os.path.basename(t)
    cand = by_suffix.get("/" + base)
    if cand and len(cand) == 1:
        return cand[0]
    return None


def theme_groups(rows, universe):
    """主题组 = 主表「重复或重叠对象」列里的**互指**对（A 点名 B 且 B 点名 A）的连通分量。

    单向点名不入组（避免"下位指向正本"这类分层指针把全集并成一组），
    但按组统计为「组外单向点名数」，全量点名仍留在主表该列。
    """
    uset = set(universe)
    by_suffix = collections.defaultdict(list)
    for u in universe:
        parts = u.split("/")
        for i in range(1, len(parts)):
            by_suffix["/" + "/".join(parts[i:])].append(u)
    rowmap = {r["路径"]: r for r in rows}
    fwd = collections.defaultdict(set)
    srcs = collections.defaultdict(set)
    for r in rows:
        for m in PATHISH.finditer(r.get("重复或重叠对象") or ""):
            tgt = resolve_token(m.group(0), uset, by_suffix)
            if tgt and tgt != r["路径"]:
                fwd[r["路径"]].add(tgt)
                srcs[tuple(sorted([r["路径"], tgt]))].add((r["来源成稿"] or [""])[0])
    directed = sum(len(v) for v in fwd.values())
    mutual = set()
    for a, ts in fwd.items():
        for b in ts:
            if a in fwd.get(b, ()):
                mutual.add(tuple(sorted([a, b])))
    adj = collections.defaultdict(set)
    for a, b in mutual:
        adj[a].add(b)
        adj[b].add(a)
    comps, seen = [], set()
    for node in sorted(adj):
        if node in seen:
            continue
        stack, comp = [node], set()
        while stack:
            x = stack.pop()
            if x in comp:
                continue
            comp.add(x)
            stack.extend(sorted(adj[x] - comp))
        seen |= comp
        comps.append(sorted(comp))
    out = []
    for gi, g in enumerate(sorted(comps, key=lambda c: (-len(c), c[0])), 1):
        cand = [p for p in g
                if re.match(r"^\s*\**\s*(是|正本|本主题组?正本候选|唯一|部分)", rowmap.get(p, {}).get("是否正本", ""))]
        one_way = sorted({t for p in g for t in fwd.get(p, ()) if t not in g})
        out.append({"id": "T%02d" % gi, "members": g, "n": len(g),
                    "pairs": sorted([("|".join(k)) for k in mutual if k[0] in g and k[1] in g]),
                    "batch_of_pair": {"|".join(k): sorted(v, key=bkey) for k, v in srcs.items()
                                      if k[0] in g and k[1] in g},
                    "正本候选": cand, "组外单向点名": one_way,
                    "themes": {p: rowmap.get(p, {}).get("主题", "") for p in g}})
    return out, directed, len(mutual)


# ---------------------------------------------------------------- disposition
DISP = ["保留", "合并", "删除", "迁移"]


def disposition(text):
    if not text.strip():
        return "未登记"
    pos = []
    for k in DISP:
        i = text.find(k)
        if i >= 0:
            pos.append((i, k))
    if not pos:
        return "其他"
    return min(pos)[1]


def dir_key(path):
    if "/" not in path:
        return "仓库根"
    seg = path.split("/")
    if seg[0] == "docs":
        return "docs/" + (seg[1] if len(seg) > 2 else "（docs 根）")
    if seg[0] in ("实验", "工程控制"):
        return seg[0] + "/" + (seg[1] if len(seg) > 2 else "（根）")
    return seg[0]


# ---------------------------------------------------------------- hotspots
# 正稿与补派记为同一批（成稿仍是两份，追溯列不变）
BATCH_OF = {"DB-10-补": "DB-10", "D1补三份": "D1残余"}


def hotspots(rows):
    cnt = collections.defaultdict(lambda: {"batches": set(), "cols": set(), "hits": 0, "who": collections.Counter()})
    for r in rows:
        for col in ("写法违规", "悬空引用", "与上位冲突"):
            txt = r.get(col) or ""
            for m in PATHISH.finditer(txt):
                tok = re.sub(r"^\.?/", "", m.group(0).replace("\\", "/"))
                tok = re.sub(r"[:：]\d[\d,\-–~ ]*$", "", tok)
                if tok == r["路径"]:
                    continue            # self reference is the row's own defect, not a hotspot object
                if "/" not in tok or len(tok) < 5:
                    continue
                e = cnt[tok]
                e["hits"] += 1
                e["cols"].add(col)
                for b in r["来源成稿"]:
                    e["who"][b] += 1
                    e["batches"].add(b)
    out = []
    for tok, e in cnt.items():
        bset = sorted(e["batches"], key=bkey)
        grs = sorted({BATCH_OF.get(b, b) for b in bset})
        out.append({"object": tok, "batches": bset, "n_batches": len(bset),
                    "groups": grs, "n_groups": len(grs),
                    "cols": sorted(e["cols"]), "hits": e["hits"],
                    "tracked": None})
    out.sort(key=lambda x: (-x["n_groups"], -x["hits"], x["object"]))
    return out


# ---------------------------------------------------------------- main
def main():
    inst = json.load(open(os.path.join(WORK, "instances.json"), encoding="utf-8"))
    universe = [l.strip() for l in open(os.path.join(INV, "DA-all.txt"), encoding="utf-8") if l.strip()]
    # (成稿, 列) 整列为空 => 该批未出该列
    col_has = collections.defaultdict(int)
    for i in inst:
        for f in FIELDS:
            if (i["fields"].get(f) or "").strip():
                col_has[(i["batch"], f)] += 1
    n_by_batch = collections.Counter(i["batch"] for i in inst)
    col_none = {(b, f) for b, n in n_by_batch.items() for f in FIELDS if col_has[(b, f)] == 0}
    rows, conflicts = merge_rows(inst, col_none)

    # gaps: 成稿 × column with zero coverage over its own instances
    tot = collections.Counter(i["batch"] for i in inst)
    have = collections.defaultdict(set)
    for i in inst:
        for f in FIELDS:
            if (i["fields"].get(f) or "").strip():
                have[i["batch"]].add(f)
    gaps = []
    for b in sorted(tot, key=bkey):
        empty = [f for f in FIELDS if f not in have[b]]
        part = {f: tot[b] - sum(1 for i in inst if i["batch"] == b and (i["fields"].get(f) or "").strip())
                for f in FIELDS}
        part = {f: c for f, c in part.items() if c}
        gaps.append({"batch": b, "instances": tot[b], "整列未出": empty, "部分缺": part})

    groups, directed_ov, mutual_ov = theme_groups(rows, universe)
    disp = collections.Counter()
    matrix = collections.defaultdict(collections.Counter)
    for r in rows:
        d = disposition(r.get("处置建议", ""))
        r["_处置分类"] = d
        r["_目录"] = dir_key(r["路径"])
        disp[d] += 1
        matrix[r["_目录"]][d] += 1

    hot = hotspots(rows)
    trk = tracked_set()
    for h in hot:
        h["tracked"] = "跟踪" if h["object"] in trk else "非跟踪"

    idx = index_paths()
    idx_set = set(idx)
    docs_rows = [r["路径"] for r in rows if r["路径"].startswith("docs/")]
    docs_tracked = sorted(p for p in trk if p.startswith("docs/"))
    unregistered = sorted(set(docs_tracked) - idx_set)
    ghost = sorted(p for p in idx_set if p.startswith("docs/") and p not in trk)
    idx_root_only = sorted(p for p in idx_set if not p.startswith("docs/"))

    derived = {
        "整列未出": sorted(["%s|%s" % t for t in col_none]),
        "主题组": groups, "点名数_有向": directed_ov, "点名数_互指": mutual_ov,
        "处置分布": dict(disp),
        "处置矩阵": {k: dict(v) for k, v in sorted(matrix.items())},
        "热点": hot,
        "索引": {"index_total": len(idx), "docs_tracked": len(docs_tracked),
               "unregistered": unregistered, "ghost": ghost, "root_entries": len(idx_root_only)},
        "冲突": conflicts, "缺列": gaps,
    }
    json.dump(rows, open(os.path.join(WORK, "merged.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    json.dump(derived, open(os.path.join(WORK, "derived.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("rows=%d conflicts=%d groups=%d hotspots>=2batch=%d unregistered=%d ghost=%d" % (
        len(rows), len(conflicts), len(groups), sum(1 for h in hot if h["n_batches"] >= 2),
        len(unregistered), len(ghost)))
    print("disposition:", dict(disp))


main()
