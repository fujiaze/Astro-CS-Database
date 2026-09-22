#!/usr/bin/env python3
"""COMPRESS-01: 由"每 (层, 阶) 中位压缩率" × 产物真实瓦片计数, 外推到整个产物的磁盘体积.
输入: evidence/zstd_tiles_shuf0.csv (只取整瓦片 *.bin, 不含 .dense.bin 与对照块)
      evidence/fill_scan.json (真实产物逐阶瓦片计数)
输出: evidence/product_level.json + 打印表
"""
import csv, collections, json, os, statistics, sys

EV = sys.argv[1] if len(sys.argv) > 1 else "run/COMPRESS-01/evidence"
LEVELS = [-7, -5, -3, -1, 1, 3, 5, 9, 12, 15, 19, 22]
NPIX_BYTES = 512 * 512 * 4


def layer_of(fn):
    for L in ("signal", "variance", "ivar", "support"):
        if fn.startswith(L):
            return L
    return None


def order_of(fn):
    p = fn.split("_")
    if len(p) >= 3 and p[1] == "leaf":
        return 9
    if len(p) >= 3 and p[1].startswith("hier"):
        return int(p[1][4:])
    return None


rows = [r for r in csv.DictReader(open(os.path.join(EV, "zstd_tiles_shuf0.csv")))
        if layer_of(r["file"]) and order_of(r["file"]) is not None and ".dense." not in r["file"]]
ratio = collections.defaultdict(list)
for r in rows:
    ratio[(layer_of(r["file"]), order_of(r["file"]), int(r["level"]))].append(float(r["ratio"]))
med = {k: statistics.median(v) for k, v in ratio.items()}

fill = json.load(open(os.path.join(EV, "fill_scan.json")))
count = collections.Counter()
for r in fill:
    count[(r["layer"], r["order"])] += 1

out = {}
print("%-9s %5s %5s %10s %s" % ("layer", "order", "n", "MiB", " ".join("%8s" % ("z%d" % l) for l in LEVELS)))
tot = collections.Counter()
for l in ("signal", "variance", "ivar", "support"):
    for o in range(0, 10):
        n = count[(l, o)]
        if not n:
            continue
        mib = n * NPIX_BYTES / 2**20
        line = "%-9s %5d %5d %10.1f " % (l, o, n, mib)
        for lv in LEVELS:
            m = med.get((l, o, lv))
            line += ("%8.4f" % m) if m is not None else "%8s" % "-"
            if m is not None:
                tot[lv] += m * n * NPIX_BYTES
        print(line)
        out.setdefault(l, {})[o] = dict(n=n, mib=mib,
                                        ratio={lv: med.get((l, o, lv)) for lv in LEVELS})
total_bytes = sum(count[k] for k in count) * NPIX_BYTES
print("\n产物瓦片 %d, 数据段总量 %.0f MiB (%.2f GiB)" % (sum(count.values()), total_bytes / 2**20, total_bytes / 2**30))
print("外推整产物数据段压缩率 (按真实瓦片计数加权):")
for lv in LEVELS:
    print("  zstd %-3d -> %.4f   (%.2f GiB -> %.2f GiB, 省 %.2f GiB)" % (
        lv, tot[lv] / total_bytes, total_bytes / 2**30, tot[lv] / 2**30,
        (total_bytes - tot[lv]) / 2**30))
json.dump(dict(per_layer=out, total_bytes=total_bytes,
               extrapolated={str(lv): tot[lv] / total_bytes for lv in LEVELS}),
          open(os.path.join(EV, "product_level.json"), "w"), indent=1)
