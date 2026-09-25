# -*- coding: utf-8 -*-
"""D8 聚合 · 步骤3：两层联接质量体检 + 主表候选导出。
用法：cd 产出 && python -B 复算/d8agg/s3_join.py
"""
import sys, os, re, csv, json, collections
sys.path.insert(0, "复算/d8agg")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
import s2_index as M

rows, reviews, objs, stat = M.build()

# 复核件 -> 其覆盖的 L1 源
REVIEW_SRC = {
    "复核-AUD201.md": ["AUD-201-测光核验.md"],
    "复核-AUD202.md": ["AUD-202-SNR核验.md"],
    "复核-AUD202-补.md": ["AUD-202-SNR核验.md"],
    "复核-AUD203.md": ["AUD-203-天光无缝核验.md"],
    "复核-AUD204.md": ["AUD-204-面积交叠核验.md"],
    "复核-DB13.md": ["AUD-101-DB-13.md"],
    "复核-合同层.md": ["AUD-101-DB-19.md", "AUD-101-DB-20.md", "AUD-101-DB-11.md"],
    "复核-架构接线.md": ["AUD-401-架构对齐.md"],
    "复核-测光默认阶数.md": ["AUD-101-DB-18.md"],
    "复核-结果层与收口层.md": ["AUD-101-DB-17.md", "AUD-101-DB-20.md"],
    "复核-负责人面与索引.md": ["AUD-101-DB-19.md", "AUD-101-DA01-根规范与科学.md"],
    "复核-门禁入口.md": ["AUD-501-门禁现状审计.md"],
}

# 对象 -> 复核小节（含判定）
def secs_for(key):
    out = {}
    for r in objs[key]["l2"]:
        s = r.get("sec")
        if not s:
            continue
        k = (r["src"], s)
        out.setdefault(k, {"verdict": r.get("secverdict"), "cls": r.get("seccls"),
                           "grade": r.get("secgrade"), "n": 0,
                           "srcs": set()})
        out[k]["n"] += 1
        out[k]["srcs"].add(r["src"])
    return out

# L1 侧：对象 -> 出现的源
def l1src(key):
    return collections.Counter(r["src"] for r in objs[key]["l1"])

both = [k for k, v in objs.items() if v["l1"] and v["l2"] and not k.startswith("__NOISE__")]
print("双层对象数:", len(both))

# 判定分布
clsdist = collections.Counter()
mixedcls = 0
for k in both:
    cs = {v["cls"] for v in secs_for(k).values() if v["cls"]}
    clsdist[tuple(sorted(cs))] += 1
print("对象级判定集合分布:")
for c, n in clsdist.most_common():
    print("   %-40s %4d" % (str(c), n))

# 只看 L1 源与复核覆盖源真正同源的对象（严口径）
def strict(k):
    ls = set(l1src(k))
    rs = set()
    for (fn, s), v in secs_for(k).items():
        rs |= set(REVIEW_SRC.get(fn, []))
    return ls & rs

s_both = [k for k in both if strict(k)]
print()
print("严口径（L1 源本身被该复核件覆盖）双层对象数:", len(s_both))
sc = collections.Counter()
for k in s_both:
    cs = {v["cls"] for v in secs_for(k).values() if v["cls"] and
          any(fn in [f for f, m in REVIEW_SRC.items() for s in m if s in strict(k)]
              for (fn, s) in secs_for(k))}
    sc[tuple(sorted(cs))] += 1
for c, n in sc.most_common():
    print("   %-30s %4d" % (str(c), n))

print()
print("=== 宽口径双层对象 top 40（按 L1 行数）===")
both.sort(key=lambda k: -len(objs[k]["l1"]))
for k in both[:40]:
    ss = secs_for(k)
    print("  %-56s l1=%-4d 复核小节=%s" % (k[:56], len(objs[k]["l1"]),
          ", ".join("%s:%s" % (s, (v["cls"] or "?")[:4]) for (f, s), v in list(ss.items())[:6])))

print()
print("=== 被复核判 VOID 的对象（作废表候选）===")
void = [k for k in both if any(v["cls"] == "VOID" for v in secs_for(k).values())]
for k in sorted(void, key=lambda x: -len(objs[x]["l1"]))[:30]:
    print("  %-56s l1=%-4d %s" % (k[:56], len(objs[k]["l1"]),
          [ (s, v["cls"]) for (f,s),v in secs_for(k).items() ]))
print("VOID 相关对象总数:", len(void))

json.dump({"both": both, "s_both": s_both},
          open("复算/d8agg/_join.json", "w", encoding="utf-8"), ensure_ascii=False)
