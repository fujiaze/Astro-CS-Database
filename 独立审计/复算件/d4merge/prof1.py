import csv, io, collections, os, sys, re

BASE = r"产出/"
out = io.open(os.path.join(BASE, r"复算/d4merge/prof1.txt"), "w", encoding="utf-8")

def W(*a):
    out.write(" ".join(str(x) for x in a) + "\n")

def load(p):
    with io.open(p, encoding="utf-8-sig", newline="") as f:
        r = list(csv.reader(f))
    return r[0], r[1:]

hdr, body = load(os.path.join(BASE, "raw/AUD-402-常数台账.csv"))
W("=== 主表 ===")
W("header:", hdr)
W("rows:", len(body))
W("colcount:", collections.Counter(len(x) for x in body))

for i, name in enumerate(hdr):
    W("--- 列 %d %s : 去重值数 %d" % (i, name, len(set(x[i] for x in body))))

W("\n=== 处置 分布 ===")
for k, v in collections.Counter(x[9] for x in body).most_common():
    W("%6d | %s" % (v, k))

W("\n=== 来源现状 分布 (全量) ===")
for k, v in collections.Counter(x[7] for x in body).most_common():
    W("%6d | %s" % (v, k))

W("\n=== 适用域 分布 top40 ===")
for k, v in collections.Counter(x[10] for x in body).most_common(40):
    W("%6d | %s" % (v, k[:80]))

# 目录族
def fam(p):
    p = p.split(";")[0].strip()
    p = p.replace("\\", "/")
    if ":" in p:
        p = p.split(":")[0]
    parts = p.split("/")
    return "/".join(parts[:3]) if len(parts) > 3 else p

W("\n=== 目录族 (位置首段前3级) top40 ===")
for k, v in collections.Counter(fam(x[1]) for x in body).most_common(40):
    W("%6d | %s" % (v, k))

W("\n=== 备注 分布 top40 ===")
for k, v in collections.Counter(x[11] for x in body).most_common(40):
    W("%6d | %s" % (v, k[:100]))

# 旁表
shdr, sbody = load(os.path.join(BASE, "raw/AUD-402-常数台账.旁表-无名值与测试钉值.csv"))
W("\n=== 旁表 ===")
W("rows:", len(sbody), "colcount:", collections.Counter(len(x) for x in sbody))
W("处置:")
for k, v in collections.Counter(x[9] for x in sbody).most_common():
    W("%6d | %s" % (v, k))
W("来源现状:")
for k, v in collections.Counter(x[7] for x in sbody).most_common(10):
    W("%6d | %s" % (v, k))
W("适用域:")
for k, v in collections.Counter(x[10] for x in sbody).most_common(10):
    W("%6d | %s" % (v, k))

out.close()
print("ok")
