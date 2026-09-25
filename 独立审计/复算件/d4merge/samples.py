import csv, io, os, re

BASE = r"产出/"
def load(p):
    with io.open(os.path.join(BASE, p), encoding="utf-8-sig", newline="") as f:
        r = list(csv.reader(f))
    return r[0], r[1:]

out = io.open(os.path.join(BASE, r"复算/d4merge/samples.txt"), "w", encoding="utf-8")
_, a1 = load("raw/AUD-402-判读-A1.csv")
_, a2 = load("raw/AUD-402-判读-A2.csv")
_, a3 = load("raw/AUD-402-判读-A3.csv")
_, bd = load("raw/AUD-402-判读-BD1.csv")

def show(tag, rows, idxs):
    for i in idxs:
        r = rows[i]
        out.write("\n===== %s row %d =====\n" % (tag, i))
        for k, h in enumerate(["符号", "位置", "值", "单位", "坐标系", "精度", "有限域", "来源现状", "出处", "处置", "适用域", "备注"]):
            out.write("  %s = %s\n" % (h, r[k]))

show("A1", a1, [0, 1, 16, 30, 55])
show("A2", a2, [0, 5, 20])
show("A3", a3, [0, 30, 60, 100])
show("BD", bd, [0, 1, 20, 40, 49])
out.close()
print("ok")
