#!/usr/bin/env python3
"""COMPRESS-01 最终数字汇总 (报告 §2/§3 的唯一数字来源).
输出 evidence/final_numbers.json + 打印报告用表格.
"""
import csv, collections, json, os, statistics, sys

EV = sys.argv[1] if len(sys.argv) > 1 else "run/COMPRESS-01/evidence"
NPIX = 512 * 512 * 4
FITS_BYTES = 1054080
BW_W, BW_R = 419.0, 359.0     # MiB/s, 本机 O_DIRECT 实测中位


def lay(f):
    for L in ("signal", "variance", "ivar", "support"):
        if f.startswith(L):
            return L


def order(f):
    p = f.split("_")
    if len(p) < 2:
        return None
    if p[1] == "leaf":
        return 9
    if p[1].startswith("hier"):
        return int(p[1][4:])
    return None


def med_by_group(path, levels, key="ratio", scale=1.0):
    rows = [r for r in csv.DictReader(open(os.path.join(EV, path)))
            if lay(r["file"]) and order(r["file"]) is not None and ".dense." not in r["file"]]
    g = collections.defaultdict(list)
    for r in rows:
        g[(lay(r["file"]), order(r["file"]), int(r["level"]))].append(float(r[key]) * scale)
    return {k: statistics.median(v) for k, v in g.items()}


def med_fits(codec, key="comp"):
    rows = [r for r in csv.DictReader(open(os.path.join(EV, "fits_codecs.csv")))
            if ".dense." not in r["file"] and r["status"] == "0" and r["codec"] == codec]
    g = collections.defaultdict(list)
    for r in rows:
        g[(lay(r["file"]), order(r["file"]))].append(float(r[key]) / FITS_BYTES * (FITS_BYTES / NPIX))
    return {k: statistics.median(v) for k, v in g.items()}


fill = json.load(open(os.path.join(EV, "fill_scan.json")))
cnt = collections.Counter((r["layer"], r["order"]) for r in fill)
tot = sum(cnt.values()) * NPIX


def extrap(med, lv=None):
    s = n = 0
    for (l, o), c in cnt.items():
        k = (l, o, lv) if lv is not None else (l, o)
        if k in med:
            s += med[k] * c * NPIX
            n += c * NPIX
    return s / n


out = {"product_bytes": tot, "tiles": sum(cnt.values()), "schemes": {}}
print("整产物: %d 瓦片, 数据段 %.2f GiB" % (sum(cnt.values()), tot / 2**30))
print("%-26s %8s %8s %9s %9s %11s %11s" % ("scheme", "ratio", "省GiB", "c_ms", "d_ms", "净ms/瓦片", "临界MiB/s"))
for path, lv, name, ck, dk in (
        ("zstd_tiles_shuf0.csv", 1, "zstd 1 (no shuffle)", "c_ms", "d_ms"),
        ("zstd_tiles_shuf0.csv", 3, "zstd 3 (no shuffle)", "c_ms", "d_ms"),
        ("zstd_tiles_shuf0.csv", 9, "zstd 9 (no shuffle)", "c_ms", "d_ms"),
        ("zstd_tiles_shuf0.csv", 19, "zstd 19 (no shuffle)", "c_ms", "d_ms"),
        ("zstd_tiles_shuf4.csv", -1, "zstd -1 + shuffle", "c_ms", "d_ms"),
        ("zstd_tiles_shuf4.csv", 1, "zstd 1 + shuffle", "c_ms", "d_ms"),
        ("zstd_tiles_shuf4.csv", 3, "zstd 3 + shuffle", "c_ms", "d_ms"),
        ("zstd_tiles_shuf4.csv", 9, "zstd 9 + shuffle", "c_ms", "d_ms"),
        ("zstd_tiles_shuf4.csv", 19, "zstd 19 + shuffle", "c_ms", "d_ms")):
    med = med_by_group(path, [lv])
    # 时长只取"真实整瓦片"(排除 .dense.bin 与对照块), 与压缩率同口径
    rows = [r for r in csv.DictReader(open(os.path.join(EV, path)))
            if int(r["level"]) == lv and ".dense." not in r["file"] and lay(r["file"])]
    c = statistics.median([float(r[ck]) for r in rows]); d = statistics.median([float(r[dk]) for r in rows])
    rho = extrap(med, lv)
    saved = (1 - rho) * (1 / BW_W + 1 / BW_R) * 1000
    out["schemes"][name] = dict(ratio=rho, saved_gib=(tot - rho * tot) / 2**30, c_ms=c, d_ms=d,
                                net_ms=saved - (c + d), bw_crit_1=2 * (1 - rho) / ((c + d) / 1000))
    print("%-26s %8.4f %8.2f %9.3f %9.3f %11.3f %11.1f" % (
        name, rho, (tot - rho * tot) / 2**30, c, d, saved - (c + d), 2 * (1 - rho) / ((c + d) / 1000)))
# TRUTHFUL-CONCLUSION-01（一页纸 S2-B）：本循环原先把"位相等样本数"硬编为 20/103，
# 与同一 dict 里的 corpus_n=len(rows) 不是同一来源（103 与 188 无法相互解释），
# 且硬编值无法随证据更新 —— 属"不可核的结论"。现改为从**同一批 rows 的 bitwise 列**
# 计数：分子与分母同源；缺列/空行集时置 None 并写明原因（fail-closed，不猜数）。
for codec, name in (("GZIP_1", "GZIP_1 q4 (fpack默认,有损)"),
                    ("GZIP_1_q0", "GZIP_1 q0 (无损)"),
                    ("GZIP_2", "GZIP_2 q4 (fpack默认,有损)"),
                    ("GZIP_2_q0", "GZIP_2 q0 (无损)"),
                    ("RICE_1", "RICE_1 q4 (有损)")):
    med = med_fits(codec)
    rows = [r for r in csv.DictReader(open(os.path.join(EV, "fits_codecs.csv")))
            if r["codec"] == codec and r["status"] == "0" and ".dense." not in r["file"] and lay(r["file"])]
    c = statistics.median([float(r["c_ms"]) for r in rows]); d = statistics.median([float(r["d_ms"]) for r in rows])
    rho = extrap(med)
    saved = (1 - rho) * (1 / BW_W + 1 / BW_R) * 1000
    if rows and "bitwise" in rows[0]:
        _bw = [int(float(r["bitwise"])) for r in rows if r.get("bitwise") not in (None, "")]
        bw_eq, bw_n = sum(1 for v in _bw if v == 1), len(_bw)
        bw_src = os.path.join(EV, "fits_codecs.csv") + "#bitwise"
        bw_why = None
    else:
        bw_eq, bw_n, bw_src = None, 0, None
        bw_why = "fits_codecs.csv 缺 bitwise 列或行集为空（fail-closed：不给数）"
    out["schemes"][name] = dict(ratio=rho, saved_gib=(tot - rho * tot) / 2**30, c_ms=c, d_ms=d,
                                net_ms=saved - (c + d), bw_crit_1=2 * (1 - rho) / ((c + d) / 1000),
                                bitwise_eq_corpus=bw_eq, corpus_n=bw_n,
                                bitwise_source=bw_src, bitwise_unavailable_reason=bw_why)
    print("%-26s %8.4f %8.2f %9.3f %9.3f %11.3f %11.1f" % (
        name, rho, (tot - rho * tot) / 2**30, c, d, saved - (c + d), 2 * (1 - rho) / ((c + d) / 1000)))
json.dump(out, open(os.path.join(EV, "final_numbers.json"), "w"), indent=1)
print("\n(临界带宽 = (1+R)·ΔS/(t_c+t_d), R=1 次读; 本机写 %.0f / 读 %.0f MiB/s)" % (BW_W, BW_R))