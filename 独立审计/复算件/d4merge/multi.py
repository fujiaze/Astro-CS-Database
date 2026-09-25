import csv, io, os, re, collections

BASE = r"产出/"
def load(p):
    with io.open(os.path.join(BASE, p), encoding="utf-8-sig", newline="") as f:
        return list(csv.reader(f))[1:]

mech = load("raw/AUD-402-常数台账.csv")
jud = load("raw/AUD-402-判读-A1.csv") + load("raw/AUD-402-判读-A2.csv") + \
      load("raw/AUD-402-判读-A3.csv") + load("raw/AUD-402-判读-BD1.csv")

out = io.open(os.path.join(BASE, "复算/d4merge/multi.txt"), "w", encoding="utf-8")
def W(*a): out.write(" ".join(str(x) for x in a) + "\n")

g = collections.defaultdict(list)
for r in jud:
    g[r[0]].append(r)
W("=== 判读层：出现>1 行的符号 ===")
for k, v in sorted(g.items(), key=lambda x: -len(x[1])):
    if len(v) > 1:
        vals = set(x[2] for x in v)
        W("\n符号 %-40s 行数=%d 值集数=%d" % (k, len(v), len(vals)))
        for x in v:
            W("     %-58s | 值=%-22s | 处置=%.30s" % (x[1], x[2][:22], x[9]))

W("\n\n=== 判读层键 是否都能回到机械层键 ===")
km = set((r[0], r[1]) for r in mech)
missing = [r for r in jud if (r[0], r[1]) not in km]
W("判读行无机械对应:", len(missing))
for r in missing[:20]:
    W("   ", r[0], "|", r[1])

W("\n=== 机械层(键,位)索引 ===")
mi = {(r[0], r[1]): r for r in mech}

W("\n=== 层间不一致统计 ===")
c = collections.Counter()
detail = []
for r in jud:
    m = mi.get((r[0], r[1]))
    if m is None:
        c["判读行无机械对应"] += 1
        continue
    ms, js = m[7], r[7]
    mout, jout = m[8].strip(), r[8].strip()
    mech_none = ms.startswith("无来源")
    jud_src = bool(jout) and not jout.startswith("无") and not jout.startswith("—") and not jout.startswith("不适用") and not jout.startswith("缺")
    if mech_none and jud_src:
        c["机械=无来源 而 判读给出处"] += 1
        detail.append(("A", r[0], r[1], jout[:70]))
    elif mech_none and r[9].startswith("不适用") and not jud_src:
        c["机械=无来源 而 判读判为结构性不适用"] += 1
    elif (not mech_none) and (not jud_src) and js.startswith("无"):
        c["机械有来源 而 判读判无出处"] += 1
        detail.append(("B", r[0], r[1], js[:60]))
    elif m[9] != r[9]:
        c["处置列不同"] += 1
for k, v in c.most_common():
    W("%5d | %s" % (v, k))
W("\n--- A 类明细（机械无来源→判读给出处）---")
for t in detail:
    if t[0] == "A":
        W("   %-34s %-52s %s" % (t[1][:34], t[2][:52], t[3]))
W("\n--- B 类明细（机械有来源→判读判无）---")
for t in detail:
    if t[0] == "B":
        W("   %-34s %-52s %s" % (t[1][:34], t[2][:52], t[3]))
out.close()
print("ok")
