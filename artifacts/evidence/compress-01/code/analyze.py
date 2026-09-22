#!/usr/bin/env python3
"""COMPRESS-01: 汇总基准 CSV -> 报告用表格 (markdown + json)."""
import csv, glob, json, os, statistics, sys, collections

EV = sys.argv[1] if len(sys.argv) > 1 else "run/COMPRESS-01/evidence"


def load(name):
    p = os.path.join(EV, name)
    if not os.path.exists(p):
        return []
    with open(p) as f:
        return list(csv.DictReader(f))


def layer_of(fn):
    for L in ("signal", "variance", "ivar", "support"):
        if fn.startswith(L):
            return L
    if fn.startswith("ctl_"):
        return "control"
    return "other"


def group_of(fn):
    parts = fn.split("_")
    if len(parts) >= 3 and parts[1].startswith("hier"):
        return "hier" + parts[1][4:]
    if len(parts) >= 3 and parts[1] == "leaf":
        return "leaf"
    return "other"


def med(rows, key):
    v = [float(r[key]) for r in rows if r.get(key) not in (None, "", "fail")]
    return statistics.median(v) if v else float("nan")


def summarize_zstd(path, tag):
    rows = load(path)
    if not rows:
        return None
    by = collections.defaultdict(list)
    for r in rows:
        by[(layer_of(r["file"]), group_of(r["file"]), int(r["level"]))].append(r)
    out = {}
    for (lay, grp, lv), rs in sorted(by.items()):
        out.setdefault((lay, grp), {})[lv] = dict(
            n=len(rs), ratio=med(rs, "ratio"), c_ms=med(rs, "c_ms"), d_ms=med(rs, "d_ms"),
            c_mbps=med(rs, "c_mbps"), d_mbps=med(rs, "d_mbps"),
            ok=sum(1 for r in rs if r["ok"] == "1"))
    return out


def md_zstd(summary, levels, title):
    lines = ["### " + title, "", "| layer | group | n | " + " | ".join("zstd %d" % l for l in levels) + " |",
             "|---|---|---|" + "---|" * len(levels)]
    for (lay, grp), d in sorted(summary.items()):
        n = max(v["n"] for v in d.values())
        lines.append("| %s | %s | %d | " % (lay, grp, n) + " | ".join(
            ("%.4f" % d[l]["ratio"]) if l in d else "-" for l in levels) + " |")
    lines.append("")
    lines.append("| layer | group | " + " | ".join("c %d" % l for l in levels) + " | " + " | ".join("d %d" % l for l in levels) + " |")
    lines.append("|---|---|" + "---|" * (2 * len(levels)))
    for (lay, grp), d in sorted(summary.items()):
        lines.append("| %s | %s | " % (lay, grp) + " | ".join(
            ("%.3f" % d[l]["c_ms"]) if l in d else "-" for l in levels) + " | " + " | ".join(
            ("%.3f" % d[l]["d_ms"]) if l in d else "-" for l in levels) + " |")
    lines.append("")
    lines.append("(c/d 单位 ms / 1 MiB 瓦片)")
    return "\n".join(lines)


def main():
    report = []
    levels = [-7, -5, -3, -1, 1, 3, 5, 9, 12, 15, 19, 22]
    for path, title in (("zstd_tiles_shuf0.csv", "zstd 数据段 (shuffle=0)"),
                        ("zstd_tiles_shuf4.csv", "zstd 数据段 (shuffle=4, f32 字节转置)"),
                        ("zstd_wholefits.csv", "zstd 真实整 .fits 文件")):
        s = summarize_zstd(path, title)
        if s:
            report.append(md_zstd(s, levels, title))
            # 控制块单列
            ctl = {k: v for k, v in s.items() if k[0] == "control"}
            if ctl:
                report.append("**对照/负例块 (1 MiB) 压缩率**\n")
                report.append("| file | " + " | ".join("zstd %d" % l for l in levels) + " |")
                report.append("|---|" + "---|" * len(levels))
                for (lay, grp), d in sorted(ctl.items()):
                    report.append("| %s | " % grp + " | ".join(
                        ("%.5f" % d[l]["ratio"]) if l in d else "-" for l in levels) + " |")
                report.append("")
    # FITS
    for path, title in (("fits_codecs.csv", "FITS 原生压缩 (cfitsio=fpack 内核), tile=512x512"),
                        ("fits_codecs_defaulttile.csv", "FITS 原生压缩, tile=default (fpack 默认行瓦片)")):
        rows = load(path)
        if not rows:
            continue
        by = collections.defaultdict(list)
        for r in rows:
            by[(r["codec"], layer_of(r["file"]), group_of(r["file"]))].append(r)
        lines = ["### " + title, "", "| codec | layer | group | n | comp bytes (中位) | ratio(数据段) | c_ms | d_ms | bitwise_eq |",
                 "|---|---|---|---|---|---|---|---|---|"]
        for (codec, lay, grp), rs in sorted(by.items()):
            ok = [r for r in rs if r["status"] == "0" and r["comp"] not in ("-1", "")]
            if not ok:
                lines.append("| %s | %s | %s | %d | FAIL(rc=%s) | - | - | - | - |" % (
                    codec, lay, grp, len(rs), rs[0]["status"]))
                continue
            csz = statistics.median([int(r["comp"]) for r in ok])
            lines.append("| %s | %s | %s | %d | %d | %.4f | %.3f | %.3f | %s |" % (
                codec, lay, grp, len(ok), csz, csz / 1048576.0,
                statistics.median([float(r["c_ms"]) for r in ok]),
                statistics.median([float(r["d_ms"]) for r in ok]),
                ok[0]["bitwise_eq"]))
        lines.append("")
        report.append("\n".join(lines))
    # python 对照
    rows = load("py_codecs.csv")
    if rows:
        by = collections.defaultdict(list)
        for r in rows:
            by[(r["codec"], layer_of(r["file"]), group_of(r["file"]))].append(r)
        lines = ["### python 侧对照 (zlib/lzma/zstd)", "", "| codec | layer | group | n | ratio | c_ms | d_ms | c MB/s | d MB/s |",
                 "|---|---|---|---|---|---|---|---|---|"]
        for (codec, lay, grp), rs in sorted(by.items()):
            lines.append("| %s | %s | %s | %d | %.4f | %.3f | %.3f | %.1f | %.1f |" % (
                codec, lay, grp, len(rs), med(rs, "ratio"), med(rs, "c_ms"), med(rs, "d_ms"),
                med(rs, "c_mbps"), med(rs, "d_mbps")))
        lines.append("")
        report.append("\n".join(lines))
    txt = "\n\n".join(report)
    open(os.path.join(EV, "tables.md"), "w").write(txt)
    print(txt)


main()
