#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/tools/quality/compare_products.py — 产物确定性比对口径（P26 T3；负责人裁决 4）。

用途: 比较两棵产品树(例如两次 run 的 signal/ + support/), 给出可复现、可解释的
确定性结论:
  * 默认**逐字节**比较(sha256);
  * 对**已知墙钟字段**(properties 的 hips_creation_date 等, 见 CLOCK_KEYS 清单)
    在报告中**单列**为"被忽略的差异", 不改变"其余字节完全一致"的结论;
  * FITS 数值内容提供 --tolerance 模式: 允许浮点舍入级差异, 但**必须给出可解释的
    容差依据**(float32 舍入/累加顺序变化导致的 ulp 级差异), 输出 max abs / max rel /
    分布(分位数), 超差即 FAIL。

负责人裁决 4: "忽略这一行, 其他的一样就行, 或者误差<一个可解释的值, 允许浮点计算
舍入误差" —— 本工具把"可解释的值"落成显式参数 + 依据文本 + 实测分布, 避免把真正的
数值漂移当成舍入误差放过。

用法:
  python3 eng/tools/quality/compare_products.py --a RUN1 --b RUN2 [--out DIR]
  python3 eng/tools/quality/compare_products.py --a R1 --b R2 --tolerance 1e-5
  python3 eng/tools/quality/compare_products.py --list-ignored

产物: compare_report.json + compare_summary.md(人类可读)。退出码: 0 一致(含仅墙钟差异),
1 存在实质差异, 2 参数/IO 错。纯 stdlib(自带最小 FITS 解析), 无第三方依赖。
"""
from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
import re
import struct
import sys
from pathlib import Path

SCHEMA_VERSION = 1

# ---------------------------------------------------------------- 墙钟字段清单 ----
# 清点依据
#   * IVOA HiPS 1.0 Recommendation, properties 元数据: hips_creation_date /
#     hips_update_date / hips_release_date —— 记录"生成/更新/发布"墙钟时间, 与产品
#     数值内容无关, 两次运行的同一产品必然不同。
#   * 仓库自有报告面通用时间戳键: created_utc / generated_utc / timestamp_utc。
#   * FITS 头 DATE: FITS 标准里 DATE 是"文件写入时间", 属墙钟; 而 DATE-OBS 是观测
#     时间(来自输入数据, 确定性), **不在忽略清单** —— 忽略它会把科学输入时间差异
#     藏起来。
CLOCK_KEYS_TEXT = (
    # IVOA HiPS properties 墙钟元数据
    "hips_creation_date",
    "hips_update_date",
    "hips_release_date",
    # 仓库报告面通用墙钟时间戳
    "created_utc",
    "generated_utc",
    "timestamp_utc",
    # 运行实例标识/起止时刻(与运行实例绑定, 同一产品两次运行必然不同)
    "run_id",
    "started_utc",
    "ended_utc",
)
# FITS 头忽略清单:
#   DATE    — 文件写入时间(墙钟);
#   RUNID   — 仓库 writer 写入的逐运行随机标识(非墙钟, 但同产品两次运行必然不同);
#   CHECKSUM/DATASUM — FITS 完整性校验和, 由包含 DATE/RUNID 的整条 HDU 计算,
#                      上两项被忽略后其差异被完全解释; 数据段仍逐元素比对,
#                      数据变化不会被校验和忽略掩盖。
# 明确不忽略: DATE-OBS(观测时间, 科学输入确定性字段)。
CLOCK_KEYS_FITS = ("DATE", "RUNID", "CHECKSUM", "DATASUM")
CLOCK_RATIONALE = (
    "忽略清单(文本): " + ", ".join(CLOCK_KEYS_TEXT),
    "  = IVOA HiPS properties 墙钟元数据 + 仓库报告面时间戳 + 运行实例标识/起止时刻。",
    "忽略清单(FITS 头): " + ", ".join(CLOCK_KEYS_FITS),
    "  = DATE(写入墙钟) + RUNID(逐运行随机标识) + CHECKSUM/DATASUM(由前二者派生的",
    "    完整性校验和; 数据段仍逐元素比对, 数值变化不会因忽略校验和而漏检)。",
    "明确不忽略: DATE-OBS(观测时间, 科学输入确定性字段); 绝对路径(output_dir/",
    "config_path/output_fits)与 product_sha256(由内容派生) —— 路径差异属运行簿记,",
    "由报告显式列出, 不以'墙钟'名义忽略; 比对产品树时应只取产品面。",
)

TOLERANCE_RATIONALE_DEFAULT = (
    "float32(IEEE-754 binary32) 有效精度 ~7 位十进制; 单次正确舍入 <=0.5 ulp,",
    "相对量级约 6e-8。数值归约(求和/加权平均)顺序变化导致的差异随项数 N 以",
    "O(sqrt(N)) ulp 增长: N=1e4 时约 100 ulp ~ 6e-6 相对。因此 1e-5 相对容差约等于",
    "100 倍单 ulp, 足以覆盖 float32 累加顺序变化, 又远小于任何科学量级差异。",
    "此为舍入级口径, **不是科学容差**; 每个产品的科学容差必须单独论证。",
)


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _is_fits(blob):
    return len(blob) >= 30 and (blob[:6] == b"SIMPLE" or blob[:8] == b"XTENSION")


def _is_texty(blob):
    if b"\x00" in blob[:4096]:
        return False
    try:
        blob[:4096].decode("utf-8")
        return True
    except UnicodeDecodeError:
        return False


# ------------------------------------------------------------------ FITS 解析 ----
def _parse_card(card):
    key = card[:8].decode("ascii", "replace").strip()
    if not key or key in ("COMMENT", "HISTORY") or card[:8] == b"        ":
        return None
    body = card[8:].decode("latin-1")
    if body[:1] != "=":
        return (key, None, body.strip())
    value = body[1:].split("/", 1)[0].strip()
    sq = chr(39)
    if value.startswith(sq) and value.endswith(sq) and len(value) >= 2:
        val = value[1:-1].strip()
    else:
        try:
            val = int(value)
        except ValueError:
            try:
                val = float(value.replace("D", "E").replace("d", "e"))
            except ValueError:
                val = value
    return (key, val, body.strip())


def _parse_fits_header(blob, off):
    cards = []
    while True:
        block = blob[off:off + 2880]
        if len(block) < 2880:
            return None, off
        off += 2880
        end = False
        for i in range(36):
            card = block[i * 80:(i + 1) * 80]
            if card[:8] == b"END     " or card.rstrip() == b"END":
                end = True
                break
            parsed = _parse_card(card)
            if parsed is not None:
                cards.append(parsed)
        if end:
            return cards, off


_BITPIX_FMT = {8: "b", 16: "h", 32: "i", 64: "q", -32: "f", -64: "d"}


def parse_fits(blob):
    """最小 FITS 解析器 → [{"cards", "values", "shape", "bitpix", "count", "data"}]。"""
    hdus = []
    off = 0
    while off + 2880 <= len(blob):
        cards, off = _parse_fits_header(blob, off)
        if cards is None:
            break
        values = {}
        for k, v, _raw in cards:
            values[k] = v
        bitpix = values.get("BITPIX")
        naxis = int(values.get("NAXIS", 0) or 0)
        shape = [int(values.get("NAXIS%d" % i, 0) or 0) for i in range(1, naxis + 1)]
        count = 1
        for s in shape:
            count *= s
        hdus.append({"cards": cards, "values": values, "shape": shape,
                     "bitpix": bitpix, "count": count})
        fmt = _BITPIX_FMT.get(bitpix)
        if fmt is None or count <= 0:
            break
        nbytes = count * struct.calcsize(fmt)
        raw = blob[off:off + nbytes]
        if len(raw) < nbytes:
            break
        arr = array.array(fmt)
        arr.frombytes(raw)
        if sys.byteorder == "little":
            arr.byteswap()
        hdus[-1]["data"] = arr
        off += ((nbytes + 2879) // 2880) * 2880
    return hdus


# ------------------------------------------------------------- 数值数组比对 ----
def compare_arrays(xs, ys, rel_tol, abs_tol):
    n = min(len(xs), len(ys))
    if len(xs) != len(ys):
        return {"length_mismatch": [len(xs), len(ys)], "n": n, "exceed": None}
    abs_diffs = []
    max_abs = 0.0
    max_rel = 0.0
    exceed = 0
    nan_mismatch = 0
    for i in range(n):
        a = xs[i]
        b = ys[i]
        a_nan = isinstance(a, float) and math.isnan(a)
        b_nan = isinstance(b, float) and math.isnan(b)
        if a_nan or b_nan:
            if a_nan != b_nan:
                nan_mismatch += 1
            continue
        d = abs(float(a) - float(b))
        scale = abs(float(b))
        abs_diffs.append(d)
        if d > max_abs:
            max_abs = d
        if scale > 0:
            r = d / scale
            if r > max_rel:
                max_rel = r
        if d > (abs_tol + rel_tol * scale):
            exceed += 1
    abs_diffs.sort()

    def pct(q):
        if not abs_diffs:
            return 0.0
        return abs_diffs[min(len(abs_diffs) - 1, int(q * (len(abs_diffs) - 1)))]

    return {"length_mismatch": None, "n": n, "max_abs": max_abs, "max_rel": max_rel,
            "exceed": exceed, "nan_mismatch": nan_mismatch,
            "abs_p50": pct(0.50), "abs_p99": pct(0.99), "abs_p999": pct(0.999)}


def compare_fits(blob_a, blob_b, rel_tol, abs_tol, ignore_fits):
    ha = parse_fits(blob_a)
    hb = parse_fits(blob_b)
    if len(ha) != len(hb):
        return {"reason": "hdu_count_mismatch", "n_hdu_a": len(ha), "n_hdu_b": len(hb)}
    ignored = []
    details = []
    worst = {"max_abs": 0.0, "max_rel": 0.0, "exceed": 0, "nan_mismatch": 0,
             "abs_p50": 0.0, "abs_p99": 0.0, "abs_p999": 0.0, "n": 0}
    structural = ("SIMPLE", "XTENSION", "BITPIX", "NAXIS", "EXTEND", "PCOUNT",
                  "GCOUNT", "TFIELDS", "END")
    for i, (da, db) in enumerate(zip(ha, hb)):
        va, vb = da["values"], db["values"]
        for key in sorted(set(va) | set(vb)):
            if key in ignore_fits:
                if va.get(key) != vb.get(key):
                    ignored.append({"key": key, "a": va.get(key), "b": vb.get(key)})
                continue
            if key in structural:
                continue
            if va.get(key) != vb.get(key):
                return {"reason": "header_mismatch", "hdu": i, "key": key,
                        "a": va.get(key), "b": vb.get(key), "ignored": ignored}
        if da["shape"] != db["shape"] or da["bitpix"] != db["bitpix"]:
            return {"reason": "structure_mismatch", "hdu": i,
                    "shape_a": da["shape"], "shape_b": db["shape"],
                    "bitpix_a": da["bitpix"], "bitpix_b": db["bitpix"],
                    "ignored": ignored}
        if "data" not in da or "data" not in db:
            continue
        st = compare_arrays(da["data"], db["data"], rel_tol, abs_tol)
        if st.get("exceed") is None:
            return {"reason": "data_length_mismatch", "hdu": i, "detail": st,
                    "ignored": ignored}
        details.append({"hdu": i, **st})
        worst["max_abs"] = max(worst["max_abs"], st["max_abs"])
        worst["max_rel"] = max(worst["max_rel"], st["max_rel"])
        worst["exceed"] += st["exceed"]
        worst["nan_mismatch"] += st["nan_mismatch"]
        worst["abs_p999"] = max(worst["abs_p999"], st["abs_p999"])
        worst["n"] += st["n"]
    if worst["exceed"] > 0 or worst["nan_mismatch"] > 0:
        return {"reason": "tolerance_exceeded", "detail": worst, "per_hdu": details,
                "ignored": ignored}
    return {"reason": "within_tolerance", "detail": worst, "per_hdu": details,
            "ignored": ignored}


# ------------------------------------------------------------- 文本墙钟忽略 ----
_KEY_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*[:=,]\s*(.*)$")


def mask_clock_text(text, keys):
    """把已知墙钟键的值替换为 <IGNORED:key>; 返回 (masked_text, 命中键集合)。"""
    hits = set()
    out = []
    q = chr(34)
    for line in text.splitlines():
        probe = line.strip()
        if probe.startswith(q):
            probe = probe[1:]
        m = _KEY_RE.match(probe)
        if m and m.group(1) in keys:
            hits.add(m.group(1))
            out.append("%s = <IGNORED:%s>" % (m.group(1), m.group(1)))
        else:
            out.append(line)
    return "\n".join(out), hits


def _read(path):
    return Path(path).read_bytes()


# ------------------------------------------------------------------ 主比对 ----
def compare_trees(a, b, rel_tol=0.0, abs_tol=0.0, extra_ignore=()):
    root_a, root_b = Path(a), Path(b)
    if not root_a.is_dir() or not root_b.is_dir():
        raise SystemExit("error: --a/--b must be directories")
    keys_text = set(CLOCK_KEYS_TEXT) | set(extra_ignore)
    keys_fits = set(CLOCK_KEYS_FITS) | set(extra_ignore)

    def walk(root):
        out = {}
        for p in sorted(root.rglob("*")):
            if p.is_file():
                out[str(p.relative_to(root)).replace("\\", "/")] = p
        return out

    fa, fb = walk(root_a), walk(root_b)
    report = {
        "schema_version": SCHEMA_VERSION,
        "tool": "eng/tools/quality/compare_products.py",
        "a": str(root_a.resolve()), "b": str(root_b.resolve()),
        "mode": "tolerance" if (rel_tol > 0 or abs_tol > 0) else "byte",
        "tolerance": {"rel": rel_tol, "abs": abs_tol},
        "tolerance_rationale": list(TOLERANCE_RATIONALE_DEFAULT),
        "ignored_keys": {"text": sorted(keys_text), "fits": sorted(keys_fits),
                         "rationale": list(CLOCK_RATIONALE)},
        "differences": [], "ignored_differences": [], "within_tolerance": [],
        "equal_files": [],
        "only_in_a": sorted(set(fa) - set(fb)), "only_in_b": sorted(set(fb) - set(fa)),
    }
    for rel in report["only_in_a"]:
        report["differences"].append({"path": rel, "kind": "missing_in_b"})
    for rel in report["only_in_b"]:
        report["differences"].append({"path": rel, "kind": "extra_in_b"})

    for rel in sorted(set(fa) & set(fb)):
        pa, pb = fa[rel], fb[rel]
        if _sha256(pa) == _sha256(pb):
            report["equal_files"].append(rel)
            continue
        blob_a, blob_b = _read(pa), _read(pb)
        if _is_fits(blob_a) and _is_fits(blob_b):
            res = compare_fits(blob_a, blob_b, rel_tol, abs_tol, keys_fits)
            if res["reason"] == "within_tolerance":
                keys = sorted({i["key"] for i in res["ignored"]})
                if keys:
                    report["ignored_differences"].append({
                        "path": rel, "kind": "fits", "keys": keys,
                        "detail": res.get("detail"),
                        "note": "仅墙钟/运行标识头差异(数值在容差内)"})
                else:
                    report["within_tolerance"].append({
                        "path": rel, "kind": "fits",
                        "detail": res.get("detail"),
                        "note": "字节不同但数值差异在容差内(无墙钟键)"})
                    report["equal_files"].append(rel + "#numeric-within-tolerance")
            else:
                report["differences"].append({"path": rel, "kind": "fits", **res})
        elif _is_texty(blob_a) and _is_texty(blob_b):
            ta = blob_a.decode("utf-8", "replace")
            tb = blob_b.decode("utf-8", "replace")
            ma, ha = mask_clock_text(ta, keys_text)
            mb, hb = mask_clock_text(tb, keys_text)
            if ma == mb:
                report["ignored_differences"].append({
                    "path": rel, "kind": "text", "keys": sorted(ha | hb),
                    "note": "仅已知墙钟字段差异, 其余逐字节一致"})
            else:
                report["differences"].append({
                    "path": rel, "kind": "text", "reason": "content_mismatch",
                    "a_sha256": _sha256(pa), "b_sha256": _sha256(pb)})
        else:
            report["differences"].append({
                "path": rel, "kind": "binary", "reason": "byte_mismatch",
                "a_sha256": _sha256(pa), "b_sha256": _sha256(pb)})

    counts = {
        "files_a": len(fa), "files_b": len(fb),
        "common": len(set(fa) & set(fb)),
        "equal": len(report["equal_files"]),
        "ignored_clock_only": len(report["ignored_differences"]),
        "numeric_within_tolerance": len(report["within_tolerance"]),
        "differing": len(report["differences"]),
        "only_in_a": len(report["only_in_a"]), "only_in_b": len(report["only_in_b"]),
    }
    if counts["differing"] == 0 and counts["only_in_a"] == 0 and counts["only_in_b"] == 0:
        verdict = ("IDENTICAL_AFTER_IGNORES"
                   if (counts["ignored_clock_only"] or counts["numeric_within_tolerance"])
                   else "IDENTICAL")
    else:
        verdict = "DIFFER"
    report["counts"] = counts
    report["verdict"] = verdict
    return report


def summarize_md(report):
    r = report
    lines = ["# 产物比对报告 (compare_products.py)", "",
             "- A: %s" % r["a"], "- B: %s" % r["b"],
             "- 模式: %s (rel=%g abs=%g)" % (r["mode"], r["tolerance"]["rel"],
                                             r["tolerance"]["abs"]),
             "- 结论: **%s**" % r["verdict"], ""]
    c = r["counts"]
    lines += ["## 计数", "", "| 项 | 值 |", "|---|---|",
              "| A 文件数 | %d |" % c["files_a"],
              "| B 文件数 | %d |" % c["files_b"],
              "| 公有文件 | %d |" % c["common"],
              "| 逐字节一致 | %d |" % c["equal"],
              "| 仅墙钟差异(已忽略) | %d |" % c["ignored_clock_only"],
              "| 数值在容差内(未忽略) | %d |" % c["numeric_within_tolerance"],
              "| 实质差异 | %d |" % c["differing"],
              "| 仅 A 有 | %d |" % c["only_in_a"],
              "| 仅 B 有 | %d |" % c["only_in_b"], ""]
    lines += ["## 被忽略的差异(墙钟字段)", "",
              "忽略清单(文本): " + ", ".join(r["ignored_keys"]["text"]),
              "忽略清单(FITS 头): " + ", ".join(r["ignored_keys"]["fits"]), ""]
    for item in r["ignored_differences"]:
        lines.append("- %s [%s] keys=%s — %s" % (
            item["path"], item["kind"], ",".join(item["keys"]) or "-",
            item.get("note", "")))
    if not r["ignored_differences"]:
        lines.append("- (无)")
    lines += ["", "## 数值在容差内(未忽略, 仅 --tolerance 模式)", ""]
    for item in r.get("within_tolerance", []):
        d = item.get("detail") or {}
        lines.append("- %s [%s] max_abs=%s max_rel=%s exceed=%s — %s" % (
            item["path"], item["kind"], d.get("max_abs"), d.get("max_rel"),
            d.get("exceed"), item.get("note", "")))
    if not r.get("within_tolerance"):
        lines.append("- (无)")
    lines += ["", "## 实质差异", ""]
    for d in r["differences"]:
        lines.append("- %s [%s] %s" % (d.get("path"), d.get("kind"),
                                       d.get("reason", d.get("kind"))))
    if not r["differences"]:
        lines.append("- (无)")
    lines += ["", "## 容差依据", ""] + ["- " + x for x in r["tolerance_rationale"]]
    lines += ["", "## 墙钟字段依据", ""] + ["- " + x for x in r["ignored_keys"]["rationale"]]
    return "\n".join(lines) + "\n"


def build_parser():
    ap = argparse.ArgumentParser(description="ACSD 产物确定性比对(P26 T3)")
    ap.add_argument("--a", required=False, help="产品树 A(如 signal/ 或整棵 run 输出)")
    ap.add_argument("--b", required=False, help="产品树 B")
    ap.add_argument("--out", default="run/resource/compare", help="报告输出目录")
    ap.add_argument("--tolerance", type=float, default=0.0,
                    help="相对容差(默认 0 = 逐字节)")
    ap.add_argument("--abs-tolerance", type=float, default=0.0, help="绝对容差")
    ap.add_argument("--tolerance-rationale", default=None, help="覆盖默认容差依据文本")
    ap.add_argument("--ignore", action="append", default=[], help="追加忽略键(可重复)")
    ap.add_argument("--list-ignored", action="store_true", help="打印墙钟忽略清单")
    return ap


def main(argv=None):
    args = build_parser().parse_args(argv)
    if args.list_ignored:
        print(json.dumps({"text": list(CLOCK_KEYS_TEXT), "fits": list(CLOCK_KEYS_FITS),
                          "rationale": list(CLOCK_RATIONALE),
                          "tolerance_rationale": list(TOLERANCE_RATIONALE_DEFAULT)},
                         ensure_ascii=False, indent=2))
        return 0
    if not args.a or not args.b:
        print("error: --a and --b are required", file=sys.stderr)
        return 2
    try:
        report = compare_trees(args.a, args.b, args.tolerance, args.abs_tolerance,
                               args.ignore)
    except OSError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2
    if args.tolerance_rationale:
        report["tolerance_rationale"] = [args.tolerance_rationale]
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "compare_report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (out / "compare_summary.md").write_text(summarize_md(report), encoding="utf-8")
    print("[compare_products] verdict=%s equal=%d ignored_clock=%d within_tol=%d diff=%d "
          "onlyA=%d onlyB=%d"
          % (report["verdict"], report["counts"]["equal"],
             report["counts"]["ignored_clock_only"],
             report["counts"]["numeric_within_tolerance"],
             report["counts"]["differing"],
             report["counts"]["only_in_a"], report["counts"]["only_in_b"]))
    for d in report["differences"][:20]:
        print("DIFF %s [%s] %s" % (d.get("path"), d.get("kind"), d.get("reason", "")))
    return 0 if report["verdict"] != "DIFFER" else 1


if __name__ == "__main__":
    sys.exit(main())
