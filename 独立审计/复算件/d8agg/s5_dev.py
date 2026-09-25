# -*- coding: utf-8 -*-
"""D8 聚合 · 步骤5：抽取四份科学核验报告的 DEV 行 + 门禁设计的门规行，
形成"主张级"任务候选（每行自带 位置/现状/应为/整改/入库）。
用法：cd 产出 && python -B 复算/d8agg/s5_dev.py
"""
import sys, os, re, csv, glob, collections
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.path.insert(0, "复算/d8agg")
import s2_index as M

PKG = "独立审计包"
# basename -> 跟踪集全路径 索引（解析"包内短路径"）
BN = collections.defaultdict(set)
for t in M.tracked:
    BN[os.path.basename(t)].add(t)
ANYPATH = re.compile(
    r"(?<![\w./-])((?:[\w.\-一-鿿]+/)*?[\w.\-一-鿿]+\.(?:md|cpp|h|hpp|json|py|yaml|yml|csv|txt|cmake|inc|def|sh|ps1|toml|jsonl|ipp|cxx|in))(?::\d+(?:\s*[-–,，]\s*\d+)?)?")


def resolve(text):
    """任意路径形态 -> 仓库全路径集合。歧义者以 `?` 前缀登记，不猜。"""
    ok, amb = set(), set()
    for m in ANYPATH.finditer(text or ""):
        p = m.group(1).lstrip("./")
        if p in M.tracked_set:
            ok.add(p)
            continue
        base = p.split("/")[-1]
        cand = BN.get(base)
        if cand and len(cand) == 1:
            ok.add(next(iter(cand)))
        elif cand:
            amb.add(base + "?" + "|".join(sorted(cand))[:60])
        else:
            amb.add("MISS:" + p)
    return ok, amb



def table_rows(fn, id_pat):
    """返回 [(line, {col: val})]；表头 = 该表 `|---|` 分隔行的上一行。"""
    lines = open(fn, encoding="utf-8").read().splitlines()
    sep = [i for i, l in enumerate(lines) if re.match(r"^\s*\|[\s|:-]*-{3}.*\|\s*$", l)
           and set(l) <= set("|-: \t")]
    hdr_at = {i: i - 1 for i in sep if i > 0 and lines[i - 1].lstrip().startswith("|")}

    def hdr_for(idx):
        prev = [h for h in sorted(hdr_at) if h < idx]
        if not prev:
            return None
        h = hdr_at[prev[-1]]
        return [c.strip().strip("*") for c in lines[h].strip().strip("|").split("|")]

    out = []
    for i, ln in enumerate(lines):
        if re.match(r"^\|\s*" + id_pat + r"\s*\|", ln):
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            hdr = hdr_for(i)
            if not hdr or len(hdr) != len(cells):
                hdr = hdr[:len(cells)] if hdr and len(hdr) > len(cells) else \
                    ["c%d" % k for k in range(len(cells))]
            out.append((i + 1, dict(zip(hdr, cells))))
    return out


recs = []
for fn, pat, tag in [("02_科学/测光链路核验报告.md", r"DEV-\d+", "测光"),
                     ("02_科学/SNR链路核验报告.md", r"DEV-\d+", "SNR"),
                     ("02_科学/天光无缝核验报告.md", r"DEV-\d+", "天光"),
                     ("02_科学/面积交叠核验报告.md", r"DEV-\d+", "面积")]:
    for lineno, d in table_rows(os.path.join(PKG, fn), pat):
        txt = " ‖ ".join("%s=%s" % (k, v) for k, v in d.items())
        loc = next((v for k, v in d.items() if "位置" in k or "落点" in k or "对象" in k), "")
        p_obj, amb1 = resolve(loc)
        p_all, amb2 = resolve(txt)
        recs.append({"域": "D3-" + tag, "编号": list(d.values())[0], "报告": fn, "行": lineno,
                     "对象": ";".join(sorted(p_obj) or sorted(p_all)[:4]),
                     "全部对象": ";".join(sorted(p_all)),
                     "歧义对象": ";".join(sorted(amb1 | amb2))[:300],
                     "标题": loc[:160],
                     "正文": txt[:2200]})

with open("复算/d8agg/out_dev_rows.csv", "w", newline="", encoding="utf-8-sig") as f:
    d = csv.DictWriter(f, fieldnames=list(recs[0].keys()))
    d.writeheader()
    d.writerows(recs)
print("DEV 主张行:", len(recs))
print("按域:", collections.Counter(r["域"] for r in recs))
print("有可解析跟踪对象的行:", sum(1 for r in recs if r["对象"]))
print("零对象的行:", [r["编号"] + "@" + r["域"] for r in recs if not r["对象"]])

obj2dev = collections.defaultdict(list)
for r in recs:
    for p in (r["全部对象"].split(";") if r["全部对象"] else []):
        obj2dev[p].append("%s:%s" % (r["域"], r["编号"]))
print("\nDEV 行覆盖的跟踪对象数:", len(obj2dev))
with open("复算/d8agg/out_dev_by_object.csv", "w", newline="", encoding="utf-8-sig") as f:
    d = csv.writer(f)
    d.writerow(["对象", "DEV主张数", "主张清单"])
    for k in sorted(obj2dev, key=lambda x: -len(obj2dev[x])):
        d.writerow([k, len(obj2dev[k]), ";".join(obj2dev[k])])
for k in sorted(obj2dev, key=lambda x: -len(obj2dev[x]))[:18]:
    print("  %-52s %2d  %s" % (k[:52], len(obj2dev[k]), ";".join(obj2dev[k])[:70]))

# 与两层联接对表：DEV 行的对象是否落在"双层确认"集里
rows, reviews, objs, stat = M.build()
both = {k for k, v in objs.items() if v["l1"] and v["l2"] and not k.startswith("__NOISE__")}
cov = sum(1 for r in recs if any(p in both for p in r["全部对象"].split(";")))
print("\nDEV 行中对象命中双层集的行数: %d / %d" % (cov, len(recs)))
