import csv, io, os, re, collections, json

BASE = r"产出/"
D = os.path.join(BASE, "复算/d4merge")
HDR = ["符号/键", "位置(路径:行)", "现行值", "单位", "坐标系/归一化", "精度要求",
       "有效有限域", "来源现状", "出处", "处置", "适用域", "备注"]


def load(p):
    with io.open(os.path.join(BASE, p), encoding="utf-8-sig", newline="") as f:
        r = list(csv.reader(f))
    return r[0], r[1:]


out = io.open(os.path.join(D, "overlap.txt"), "w", encoding="utf-8")
def W(*a):
    out.write(" ".join(str(x) for x in a) + "\n")

hdr, body = load("raw/AUD-402-常数台账.csv")

def bucket(r):
    n = r[11]
    if "行内浮点字面量旁表" in n: return "G"
    if "命名常数旁表" in n: return "CD"
    return "HAND"

hand = [r for r in body if bucket(r) == "HAND"]
W("手工登记行:", len(hand))

# 队列
qa = []
for i in range(1, 12):
    _, b = load("inventory/P402-A-%02d.csv" % i)
    qa += b
qb = []
for i in range(1, 51):
    _, b = load("inventory/P402-B-%02d.csv" % i)
    qb += b
qbd = []
for i in range(1, 28):
    _, b = load("inventory/P402-BD-%02d.csv" % i)
    qbd += b
W("A队列:", len(qa), " B队列:", len(qb), " BD队列:", len(qbd), " 合计:", len(qa)+len(qb)+len(qbd))

# 判读层
j = {}
for nm in ["判读-A1", "判读-A2", "判读-A3", "判读-BD1"]:
    h, b = load("raw/AUD-402-%s.csv" % nm)
    W("%s: cols=%d rows=%d hdr匹配=%s" % (nm, len(h), len(b), h == HDR))
    j[nm] = b

judged = j["判读-A1"] + j["判读-A2"] + j["判读-A3"] + j["判读-BD1"]
W("判读层总行数:", len(judged))
W("判读层唯一(符号,位置)键:", len(set((r[0], r[1]) for r in judged)))
W("判读层唯一符号:", len(set(r[0] for r in judged)))

# A判读 是否 == A队列 键集
ka = set((r[0], r[1]) for r in qa)
kj_a = set((r[0], r[1]) for r in j["判读-A1"] + j["判读-A2"] + j["判读-A3"])
W("A队列键==A判读键:", ka == kj_a, " 队列", len(ka), "判读", len(kj_a))
kbd = set((r[0], r[1]) for r in qbd)
kj_bd = set((r[0], r[1]) for r in j["判读-BD1"])
W("BD1 是 BD 队列子集:", kj_bd <= kbd, " BD1", len(kj_bd), " BD队列", len(kbd))
W("BD1 唯一符号:", len(set(r[0] for r in j["判读-BD1"])))

# 手工行 与 判读层 的交
kh = set((r[0], r[1]) for r in hand)
W("手工行键 ∩ A判读键:", len(kh & kj_a), "/", len(kh))
W("手工行键 ∩ BD判读键:", len(kh & kj_bd))
W("手工行中不在任何判读层的键:", len(kh - kj_a - kj_bd))
for r in hand:
    if (r[0], r[1]) not in (kj_a | kj_bd):
        W("   仅手工:", r[0], "|", r[1], "|", r[9], "|", r[7][:40])

# 无名行
W("\n判读层里 符号=(无名) 的行数:", sum(1 for r in judged if r[0].strip() in ("(无名)", "", "(无名）")))
for nm in j:
    W("   %s: %d" % (nm, sum(1 for r in j[nm] if r[0].strip() in ("(无名)", ""))))

# 处置分布
for nm in list(j) + ["合计"]:
    rows = judged if nm == "合计" else j[nm]
    W("\n处置分布 %s:" % nm)
    for k, v in collections.Counter(r[9] for r in rows).most_common():
        W("   %4d | %s" % (v, k))

out.close()
print("ok")
