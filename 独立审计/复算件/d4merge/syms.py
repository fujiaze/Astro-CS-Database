import csv, io, os, collections

BASE = r"产出/"
def load(p):
    with io.open(os.path.join(BASE, p), encoding="utf-8-sig", newline="") as f:
        return list(csv.reader(f))[1:]

jud = load("raw/AUD-402-判读-A1.csv") + load("raw/AUD-402-判读-A2.csv") + \
      load("raw/AUD-402-判读-A3.csv") + load("raw/AUD-402-判读-BD1.csv")
o = io.open(os.path.join(BASE, "复算/d4merge/symbols.txt"), "w", encoding="utf-8")
named = sorted(set(r[0] for r in jud if r[0].strip() != "(无名)"))
o.write("唯一具名符号 %d\n" % len(named))
for s in named:
    rows = [r for r in jud if r[0] == s]
    o.write("%-42s n=%d  值=%s\n" % (s, len(rows), " ;; ".join(x[2][:26] for x in rows)))
o.close()
print("ok", len(named))
