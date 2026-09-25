# -*- coding: utf-8 -*-
"""为 MD 生成三类高价值清单与计数核对（全部由主表与成稿计数派生，不新增判读）。"""
import csv, io, os, re, json, collections

BASE = r"产出/"
OUT = os.path.join(BASE, "复算/d4merge")

rows = list(csv.reader(io.open(os.path.join(BASE, r"独立审计/02_科学\常数公式算法总台账.csv"),
                               encoding="utf-8-sig", newline="")))
hdr, body = rows[0], [r for r in rows[1:] if len(r) == 16]
ix = {h: i for i, h in enumerate(hdr)}

o = io.open(os.path.join(OUT, "lists.txt"), "w", encoding="utf-8")


def W(*a):
    o.write(" ".join(str(x) for x in a) + "\n")


red = [r for r in body if r[13].startswith("红")]
yel = [r for r in body if r[13].startswith("黄")]

# ① 多侧取值不一致（红且引文含"侧／入口／值不等／并存／互斥"）
SIDE = re.compile(r"六侧|三侧|两侧|多侧|两处入口|两个生产入口|并存|互斥|取值不等|三套|两套|值集不相交|分叉|相反|打脸")
c1 = [r for r in red if SIDE.search(r[13])]
# ② 该按公式算却硬编码
HARD = re.compile(r"硬编码|写死|截断值|应按式|由式生成|按式算|导出式已|唯一正确值|应写成|复用 sysconf|由 kPerNode|派生|不该写字面量|不联动|无断言|直写|9216|leaf_order|应由式|生成而非")
c2 = [r for r in body if HARD.search(r[13] + " " + r[9] + " " + r[8])
      or (r[10] == "公式导出" and re.search(r"代码字面量|字面量|直写|constexpr|#define|兜底", r[9] + " " + r[8]))]
# ③ 阈值或默认值缺来源（含"默认字面量冒充测量值"）
NOSRC = re.compile(r"无来源|无出处|未登记|零登记|无登记|悬空|不在跟踪集|不可复核|抄成观测值|冒充|观测值入规范|无唯一数值源|标定证据")
c3 = [r for r in body if NOSRC.search(r[8] + " " + r[9]) and r[10] in ("待确认", "实验标定")]
IMPERSONATE = [r for r in body if re.search(r"冒充|恒不触发|未接线|结构性占位|默认字面量", r[13] + " " + r[9])]

W("主表行数 %d；红 %d；黄 %d" % (len(body), len(red), len(yel)))
W("①多侧取值不一致 %d 条；②该导出却硬编码 %d 条；③缺来源 %d 条（其中『默认冒充测量』%d 条）"
  % (len(c1), len(c2), len(c3), len(IMPERSONATE)))

for name, lst in [("① 多侧取值不一致", c1), ("② 该按公式算却硬编码", c2),
                  ("③ 阈值/默认值缺来源", c3), ("③b 默认字面量冒充测量值", IMPERSONATE)]:
    W("\n\n########## %s（%d 条）##########" % (name, len(lst)))
    for r in lst:
        W("\n· %s ｜ %s ｜ 值=%s" % (r[0], r[1], r[3][:70]))
        W("   冲突：%s" % r[13][:340])
        W("   成稿：%s" % r[14][:150])

# 处置×类别 / 目录族
W("\n\n########## 处置分布（主表） ##########")
for k, v in collections.Counter(r[10] for r in body).most_common():
    W("  %5d %s" % (v, k))
W("########## 类别分布 ##########")
for k, v in collections.Counter(r[1] for r in body).most_common():
    W("  %5d %s" % (v, k))
W("########## 类别×处置 ##########")
for k, v in sorted(collections.Counter((r[1], r[10]) for r in body).items()):
    W("  %-4s %-5s %d" % (k[0], k[1], v))


def fam(r):
    p = (r[2].split("；")[0] if r[2] else "").split(":")[0]
    parts = p.split("/")
    if len(parts) > 2:
        return "/".join(parts[:2])
    return parts[0] if parts and parts[0] else "（无路径·成稿级条目）"


W("########## 目录族分布（按位置集合首个位点） ##########")
for k, v in collections.Counter(fam(r) for r in body).most_common():
    W("  %5d %s" % (v, k))
W("########## 目录族 × 处置 ##########")
fx = collections.Counter((fam(r), r[10]) for r in body)
top = [k for k, _ in collections.Counter(fam(r) for r in body).most_common()]
for f in top:
    W("  %-26s %s" % (f, {d: fx[(f, d)] for d in ("文献值", "公式导出", "实验标定", "待确认", "不适用") if fx[(f, d)]}))

# A3 计数核对
a3 = list(csv.reader(io.open(os.path.join(BASE, "raw/AUD-402-判读-A3.csv"), encoding="utf-8-sig", newline="")))[1:]
c4 = sum(1 for r in a3 if r[9].strip().startswith("④"))
W("\n\n########## 计数核对 ##########")
W("A3 CSV ④档行数 = %d；A3 md §3 表实际数据行 = 15；A3 md §3 标题自称 = 18 行；A3 md §1 分布自称 ④18/①②9/不适用97" % c4)
W("A3 ①②档行数 = %d；A3 不适用行数 = %d；A3 总行数 = %d"
  % (sum(1 for r in a3 if r[9].strip().startswith(("①", "②"))),
     sum(1 for r in a3 if r[9].strip().startswith("不适用")), len(a3)))
for nm in ("A1", "A2", "A3", "BD1"):
    b = list(csv.reader(io.open(os.path.join(BASE, "raw/AUD-402-判读-%s.csv" % nm),
                                encoding="utf-8-sig", newline="")))[1:]
    W("  %s 行数=%d 唯一符号=%d" % (nm, len(b), len(set(x[0] for x in b))))

# 待确认在主表中的分布
W("\n主表 处置=待确认 的行数 = %d" % sum(1 for r in body if r[10] == "待确认"))
W("其中红 = %d" % sum(1 for r in body if r[10] == "待确认" and r[13].startswith("红")))
o.close()
print("ok")
