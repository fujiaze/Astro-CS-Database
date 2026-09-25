import csv, io, collections, os, json

BASE = r"产出/"
out = io.open(os.path.join(BASE, r"复算/d4merge/prof2.txt"), "w", encoding="utf-8")
def W(*a): out.write(" ".join(str(x) for x in a) + "\n")

def load(p):
    with io.open(os.path.join(BASE, p), encoding="utf-8-sig", newline="") as f:
        r = list(csv.reader(f))
    return r[0], r[1:]

hdr, body = load("raw/AUD-402-常数台账.csv")

# buckets by 备注
def bucket(r):
    n = r[11]
    if "行内浮点字面量旁表" in n: return "G-无名行内字面量"
    if "命名常数旁表" in n: return "C/D-命名常数机械行"
    if "测试/实验钉值" in n: return "F-测试钉值"
    return "判读行(手工登记)"

W("=== 主表分桶 ===")
for k, v in collections.Counter(bucket(r) for r in body).most_common():
    W("%6d | %s" % (v, k))

W("\n=== 主表分桶 x 处置 ===")
c = collections.Counter((bucket(r), r[9]) for r in body)
for (b, d), v in sorted(c.items()):
    W("%6d | %-22s | %s" % (v, b, d))

jud = [r for r in body if bucket(r) == "判读行(手工登记)"]
W("\n=== 判读行 (n=%d) 全列 dump ===" % len(jud))
for i, r in enumerate(jud):
    W("\n--- 主表判读行 %d" % i)
    for j, h in enumerate(hdr):
        W("    %s = %s" % (h, r[j]))

# 符号重复情况
W("\n\n=== 符号聚合（全主表）===")
g = collections.defaultdict(list)
for r in body:
    g[r[0]].append(r)
W("唯一符号数:", len(g))
W("符号出现次数分布 top:", collections.Counter(len(v) for k, v in g.items()).most_common(10))
W("\n多行符号 (出现>=2) 数:", sum(1 for v in g.values() if len(v) > 1))
W("多行且现行值不等 的符号数:", sum(1 for v in g.values() if len(set(x[2] for x in v)) > 1))

# 判读层
for name in ["判读-A1", "判读-A2", "判读-A3", "判读-BD1"]:
    h, b = load("raw/AUD-402-%s.csv" % name)
    W("\n\n########## AUD-402-%s.csv  cols=%d rows=%d" % (name, len(h), len(b)))
    W("HEADER:", h)
    for i, r in enumerate(b):
        W("\n===== 行 %d =====" % i)
        for j, hh in enumerate(h):
            W("  %s = %s" % (hh, r[j] if j < len(r) else "<缺失>"))
out.close()
print("ok")
