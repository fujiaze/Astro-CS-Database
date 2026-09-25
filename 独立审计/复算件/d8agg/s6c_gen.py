# -*- coding: utf-8 -*-
"""D8 聚合 · 步骤6c：生成 W0/W1 独立任务文件 + 文件域冲突表。
用法：cd 产出 && python -B 复算/d8agg/s6c_gen.py
"""
import sys, os, re, csv, json, collections
sys.path.insert(0, "复算/d8agg")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
import s2_index as M
from s6_spec import TASKS

OUT = "独立审计/06_实施"
TASKS_DIR = os.path.join(OUT, "tasks")
os.makedirs(TASKS_DIR, exist_ok=True)

rows, reviews, objs, stat = M.build()
dev = list(csv.DictReader(open("复算/d8agg/out_dev_rows.csv", newline="", encoding="utf-8-sig")))
agg = {r["对象"]: r for r in csv.DictReader(open("复算/d8agg/out_tasks_objagg.csv",
                                                 newline="", encoding="utf-8-sig"))}
devmap = collections.defaultdict(list)
for d in dev:
    for p in [x for x in d["全部对象"].split(";") if x]:
        devmap[p].append(d)


def cells(txt):
    out = {}
    for seg in txt.split("‖")[1:]:
        if "=" in seg:
            k, v = seg.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def prof(key):
    o = objs.get(key)
    l1src, secs, anchors, l1 = collections.Counter(), {}, set(), []
    if o:
        for r in o["l1"]:
            l1src[r["src"]] += 1
            l1.append(r)
            if r.get("anchor"):
                anchors.add(r["anchor"])
        for r in o["l2"]:
            f, s = r["src"], r.get("sec")
            if not s:
                continue
            d = secs.setdefault((f, s), {"cls": r.get("seccls"), "v": r.get("secverdict"),
                                         "g": r.get("secgrade"), "n": 0})
            d["n"] += 1
            for kk in ("cls", "v", "g"):
                if r.get(kk) and not d.get(kk):
                    d[kk] = r[kk]
    return {"l1src": l1src, "secs": secs, "anchors": anchors, "l1": l1,
            "devs": devmap.get(key, [])}


def esc(x):
    return (x or "").replace("|", "/").replace("\n", " ")


def obj_table(t):
    hd = ("| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | "
          "第②层小节与判定 | 证据入库状态 |\n|---|---|---|---|---|---|---|\n")
    lines = [hd]
    for k in t["objs"]:
        pr = prof(k)
        srcs = "、".join("%s×%d" % (a, b) for a, b in pr["l1src"].most_common()) or "—"
        sv = " ".join("%s·%s[%s%s]" % (f[3:-3], s, (d["cls"] or "?")[:4], "/" + d["g"] if d["g"] else "")
                      for (f, s), d in sorted(pr["secs"].items())
                      if f.startswith("复核-") and d["cls"]) or "—"
        cur = should = ""
        if pr["devs"]:
            d0 = cells(pr["devs"][0]["正文"])
            cur = (d0.get("现行口径") or d0.get("落点") or "")[:170]
            should = (d0.get("正确口径") or d0.get("订正要求") or d0.get("整改") or "")[:200]
            if len(pr["devs"]) > 1:
                should += "（同对象另有 %d 条 D3 主张）" % (len(pr["devs"]) - 1)
        a = agg.get(k, {})
        ex = ("需复测（对象不在跟踪集）" if a.get("跟踪集实存") == "否"
              else "不可复核（读数件未入库）" if (k.startswith("artifacts/") or k.startswith("run/"))
              else "入库")
        if k.startswith("cfg:"):
            ex = "入库（六面零登记本身即证据，命令见复核件 §三）"
        lines.append("| `%s` | %s | %s | %s | %s | %s | %s |" % (
            k, esc(";".join(sorted(pr["anchors"])[:6]) or "—"), esc(cur or "（见 §2 依据）"),
            esc(should or "（见 §3 改法对应步骤）"), esc(srcs), esc(sv), ex))
    return "\n".join(lines), prof


def locus_table(t):
    lines = ["| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |", "|---|---|---|"]
    for k in t["objs"]:
        pr = prof(k)
        loci = sorted({"%s:%s" % (r["src"], r["line"]) for r in pr["l1"]})
        sv = " ‖ ".join("%s·%s：%s" % (f[3:-3], s, (d["v"] or "")[:70])
                        for (f, s), d in sorted(pr["secs"].items()) if d.get("v"))
        lines.append("| `%s` | %s | %s |" % (k, esc(";".join(loci[:12]) + ("…" if len(loci) > 12 else ""))
                                             or "—", esc(sv) or "—"))
    return "\n".join(lines)


made = []
for t in TASKS:
    if t["wave"] not in ("W0", "W1"):
        continue
    ob, _ = obj_table(t)
    b = ["# 任务：%s %s" % (t["id"], t["title"]), "",
         "> 波次 `%s` ｜ 杠杆分档 `%s` ｜ 整改域 %s ｜ 基线 HEAD `c8f64e9a`" % (t["wave"], t["tier"], t["domain"]),
         "> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。"
         "行锚与数值一律回指来源件，不在本件重述为实测。", ""]
    b += ["## 1 对象与现状 → 应为（按被修对象聚合）", "", ob,
          "", "## 2 依据", "", t["basis"], "",
          "## 3 改法（具体动作，动词开头）", ""]
    b += ["%d. %s" % (i, h) for i, h in enumerate(t["how"], 1)]
    b += ["", "## 4 文件域（本任务允许触碰的路径集合）", "", "```text"]
    b += ["%s" % k for k in t["objs"]]
    b += ["```", "",
          "不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；"
          "不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。", "",
          "## 5 与其他任务的关系", ""]
    b += ["- 顺序 / 前置："] + ["  - %s" % x for x in t["seq"]]
    if t["blocks"]:
        b += ["- 必须同批 / 文件域互斥（详表见总览 §5）："] + ["  - %s" % x for x in t["blocks"]]
    else:
        b += ["- 无跨任务硬依赖，可独立执行与验收。"]
    b += ["", "## 6 完成判据（可红可绿：注入下列之一它必须红）", ""]
    b += ["- %s" % n for n in t["neg"]]
    b += ["- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；"
          "`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。",
          "", "## 7 禁止", ""]
    b += ["- %s" % f for f in t["forbid"]]
    if t.get("note"):
        b += ["", "## 8 登记与边界", "", t["note"]]
    b += ["", "---", "", "## 来源位点全量（54 份分片成稿 + 12 份复核件）", "", locus_table(t), ""]
    safe = "".join("-" if c in '\\/:*?"<>|' else ("-" if c in "\u2013\u2014 \u00a0" else c) for c in t["short"]).strip("-")
    p = os.path.join(TASKS_DIR, "%s_%s.md" % (t["id"], safe))
    open(p, "w", encoding="utf-8").write("\n".join(b) + "\n")
    made.append(p)
print("生成任务文件:", len(made))

# ---------- 文件域两两相交 ----------
dom = {t["id"]: set(t["objs"]) for t in TASKS}
name = {t["id"]: t["short"] for t in TASKS}
pairs = []
ids = sorted(dom)
for i in range(len(ids)):
    for j in range(i + 1, len(ids)):
        a, b2 = ids[i], ids[j]
        it = dom[a] & dom[b2]
        if it:
            pairs.append((a, b2, sorted(it)))
with open("复算/d8agg/out_conflicts.csv", "w", newline="", encoding="utf-8-sig") as f:
    w = csv.writer(f)
    w.writerow(["任务A", "任务A短名", "任务B", "任务B短名", "交集对象数", "交集对象"])
    for a, b2, s in pairs:
        w.writerow([a, name[a], b2, name[b2], len(s), ";".join(s)])
print("相交任务对:", len(pairs))

# ---------- 对象归属计数 ----------
own = collections.Counter(k for t in TASKS for k in t["objs"])
hub = collections.most_common = sorted(own.items(), key=lambda x: -x[1])[:14]
json.dump({"pairs": [[a, b, s] for a, b, s in pairs], "hub": [[k, v] for k, v in hub],
           "n_objs": len(own), "n_tasks": len(TASKS)},
          open("复算/d8agg/_conf.json", "w", encoding="utf-8"), ensure_ascii=False)

# ---------- 供总览用的 markdown 冲突表 ----------
lines = ["| 任务 A | 任务 B | 交集对象（被修文件） | 处置 |", "|---|---|---|---|"]
DISP = {}
for a, b2, s in pairs:
    key = tuple(sorted((a, b2)))
    DISP.setdefault(key, "合并为同批提交（同一注册表/同一合同条款两侧不可先后改）")
top = sorted(pairs, key=lambda x: -len(x[2]))
for a, b2, s in top:
    lines.append("| %s %s | %s %s | %s（%d 个）：%s | %s |" % (
        a, name[a], b2, name[b2], "交集", len(s),
        ", ".join("`%s`" % x for x in s[:4]) + ("…" if len(s) > 4 else ""),
        "必须同批" if len(s) >= 2 else "同批或标先后"))
open("复算/d8agg/out_conflict_md.txt", "w", encoding="utf-8").write("\n".join(lines))
print("冲突表 md 片段 -> 复算/d8agg/out_conflict_md.txt  行数", len(top))

# 单对象枢纽（被 >=3 任务共享）
h3 = [(k, v) for k, v in own.items() if v >= 3]
print("被 ≥3 个任务点名的枢纽对象:", len(h3))
for k, v in sorted(h3, key=lambda x: -x[1]):
    print("   %-58s %d" % (k[:58], v))
