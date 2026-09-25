# -*- coding: utf-8 -*-
"""D8 聚合 · 步骤4：机读主表导出（对象 × 复核主张 × 源位点 × 波次）。
用法：cd 产出 && python -B 复算/d8agg/s4_emit.py
"""
import sys, os, re, csv, json, collections
sys.path.insert(0, "复算/d8agg")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
import s2_index as M

rows, reviews, objs, stat = M.build()

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
SUMMARY_SECS = {"进 P0/P1", "本批汇总", "推翻或降级", "必须负责人裁", "需负责人裁",
                "改判一览", "结论汇总", "补派结论汇总"}


def secs_for(key):
    out = {}
    for r in objs[key]["l2"]:
        s = r.get("sec")
        if not s or s in SUMMARY_SECS or re.match(r"^(附|0\.|一、|二、|三、|git|收工|开工)", s):
            pass
        k = (r["src"], s)
        d = out.setdefault(k, {"verdict": r.get("secverdict"), "cls": r.get("seccls"),
                               "grade": r.get("secgrade"), "n": 0, "title": ""})
        d["n"] += 1
        if not d["title"]:
            for sv in reviews.get(r["src"], {}).get("secs", []):
                if sv["id"] == s:
                    d["title"] = sv["title"]
    return {k: v for k, v in out.items() if v["cls"]}


# ---- 波次判定（按被修对象的整改性质，不按发现先后）
GATE_PAT = re.compile(r"^(eng/ci/|eng/tests/|eng/contracts/|eng/tools/|docs/ci/|\.github/)")
SCI_PAT = re.compile(r"^(docs/science/|docs/algorithms/|docs/plugins/|lib/algorithms/|实验/|eng/packaging/)")
CODE_PAT = re.compile(r"^(lib/infrastructure/|lib/include/|CMakeLists\.txt|docs/design/|docs/modules/|docs/interfaces/|docs/architecture/)")
CONTRACT_PAT = re.compile(r"^(docs/contracts/)")
FACE_PAT = re.compile(r"^(README\.md|docs/GLOSSARY\.md|docs/KNOWN_LIMITATIONS\.md|docs/owner/|docs/TRACEABILITY|AGENTS\.md|ENGINEERING_SPEC\.md|ACCEPTANCE_SPEC\.md|ASTROCS_DESIGN\.md|CONTROL_PACK_SPEC\.md|DEPENDENCIES\.md|memory\.md|docs/DOCUMENT_INDEX\.yaml)")


def wave_of(key):
    if key.startswith("cfg:"):
        return "W1"
    if GATE_PAT.match(key) or key in ("eng/ci/checks.json",):
        return "W0"
    if SCI_PAT.match(key):
        return "W1"
    if CONTRACT_PAT.match(key):
        return "W1"
    if CODE_PAT.match(key):
        return "W2"
    if FACE_PAT.match(key):
        return "W4" if key == "docs/DOCUMENT_INDEX.yaml" else "W3"
    return "W3"


both = [k for k, v in objs.items() if v["l1"] and v["l2"] and not k.startswith("__NOISE__")]
out_rows = []
for k in sorted(both, key=lambda x: (wave_of(x), x)):
    ss = secs_for(k)
    if not ss:
        continue
    clss = {v["cls"] for v in ss.values()}
    if "VOID" in clss and clss == {"VOID"}:
        tier = "作废"
    elif "VOID" in clss:
        tier = "对象级混判"
    elif clss <= {"PASS", "PASS_DEGRADED"}:
        tier = "主表"
    else:
        tier = "主表" if "PASS" in clss else "待加深"
    grades = sorted({v["grade"] for v in ss.values() if v["grade"]})
    srcs = collections.Counter(r["src"] for r in objs[k]["l1"])
    ls = set(srcs)
    rs = {f for (f, s) in ss}
    strict = bool(ls & {x for f in rs for x in REVIEW_SRC.get(f, [])})
    off = objs[k]["offrepo"]
    out_rows.append({
        "对象": k, "波次": wave_of(k), "层级": tier, "严格同源": "是" if strict else "否",
        "跟踪集实存": "否" if off else "是",
        "L1行数": len(objs[k]["l1"]), "L2行数": len(objs[k]["l2"]),
        "L1源": ";".join("%s×%d" % (a, b) for a, b in srcs.most_common()),
        "复核小节": " ‖ ".join("%s·%s[%s]/%s" % (f.replace("复核-", "").replace(".md", ""),
                             s, (v["cls"] or "?"), v["grade"] or "-") for (f, s), v in sorted(ss.items())),
        "小节标题": " ‖ ".join(dict.fromkeys(v["title"] for v in ss.values() if v["title"]))[:400],
        "定级建议": ";".join(grades),
        "位点L1": ";".join(dict.fromkeys("%s:%s" % (r["src"], r["line"]) for r in objs[k]["l1"]))[:600],
        "锚点": ";".join(dict.fromkeys(r["anchor"] for r in objs[k]["l1"] + objs[k]["l2"] if r.get("anchor")))[:200],
    })

w = "复算/d8agg/out_tasks_objagg.csv"
with open(w, "w", newline="", encoding="utf-8-sig") as f:
    d = csv.DictWriter(f, fieldnames=list(out_rows[0].keys()))
    d.writeheader()
    d.writerows(out_rows)
print("主表对象行:", len(out_rows), "->", w)
print(collections.Counter((r["波次"], r["层级"]) for r in out_rows))
print("波次合计:", collections.Counter(r["波次"] for r in out_rows))
print("层级合计:", collections.Counter(r["层级"] for r in out_rows))
print("严格同源:", collections.Counter(r["严格同源"] for r in out_rows))
print("跟踪集实存:", collections.Counter(r["跟踪集实存"] for r in out_rows))

# ---- 待第二层：只有 L1 的对象，按源汇总
pend = collections.defaultdict(collections.Counter)
for k, v in objs.items():
    if k.startswith("__NOISE__") or v["l2"]:
        continue
    for r in v["l1"]:
        pend[r["src"]][k] += 1
with open("复算/d8agg/out_pending_l2.csv", "w", newline="", encoding="utf-8-sig") as f:
    d = csv.writer(f)
    d.writerow(["来源成稿", "对象", "提及行数", "波次"])
    for s in sorted(pend):
        for k in sorted(pend[s], key=lambda x: -pend[s][x]):
            d.writerow([s, k, pend[s][k], wave_of(k)])
tot = {k for k, v in objs.items() if v["l1"] and not v["l2"] and not k.startswith("__NOISE__")}
print("\n待第二层对象数:", len(tot))
print("按源分片的待第二层对象数:")
for s in sorted(pend):
    print("   %-34s %4d" % (s, len(pend[s])))
print("待第二层波次分布:", collections.Counter(wave_of(k) for k in tot))

# ---- 噪声桶
print("\n=== 目录前缀级泛指（不可当对象键）===")
for k, v in objs.items():
    if k.startswith("__NOISE__"):
        print("  %-24s 未重算出行 %d 行 (源 %s)" % (k[10:], len(v["noise"]),
              sorted({r["src"] for r in v["noise"]})[:6]))
print("噪声 token 总数 %d，其中按原文重算出具体路径 %d 次"
      % (stat["noise_tok"], stat["noise_recovered"]))

# ---- 复核主张汇总（给 W0/W1 写任务书用）
with open("复算/d8agg/out_claims.md", "w", encoding="utf-8") as f:
    f.write("# 复核件主张汇总（第②层判定 + 定级 + 小节标题）\n\n")
    for fn in sorted(reviews):
        f.write("## %s （覆盖成稿：%s）\n\n" % (fn, ", ".join(REVIEW_SRC.get(fn, ["?"]))))
        for s in reviews[fn]["secs"]:
            if not s["verdict"]:
                continue
            f.write("- **%s %s** ｜判定 `%s`｜定级 `%s`\n" % (
                s["id"], s["title"], s["cls"], s["grade"] or "-"))
            f.write("  - 判定原文：%s\n" % s["verdict"][:300])
            objs_in = sorted({k for k, v in objs.items()
                              if any(r.get("sec") == s["id"] and r["src"] == fn
                                     for r in v["l2"])})
            f.write("  - 点名对象：%s\n" % (", ".join(objs_in[:24]) or "(未抽出)"))
            f.write("\n")
print("\n复核主张汇总 -> 复算/d8agg/out_claims.md")
