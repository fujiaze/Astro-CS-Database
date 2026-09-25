#!/usr/bin/env python3
"""AUD-402 ledger builder.

MAIN table  (raw/AUD-402-常数台账.csv)
  A. authored rows  : scripts/AUD-402/rows_b*.tsv  (12 tab-separated columns, no header)
  B. named numeric constants of tracked production code (auto-registered, evidence
     pulled from the declaration comment when one exists)
SIDECAR     (raw/AUD-402-常数台账.旁表-无名值与测试钉值.csv)
  C. unnamed inline floating literals in production code
  D. pinned expectations / tolerances in eng/tests and 实验 (registration only)

Column order (AUD-402 contract):
  1 符号/键 | 2 位置(路径:行) | 3 现行值 | 4 单位 | 5 坐标系/归一化 | 6 精度要求
  7 有效有限域 | 8 来源现状 | 9 出处 | 10 处置 | 11 适用域 | 12 备注
来源现状 ∈ {有文献, 可公式导出, 需实验标定, 无来源} (+ 不适用（结构性常数）)
处置 ∈ {文献值, 公式导出, 实验标定, 待确认}

Env: AUD402_MECH=0|1 (default 1), AUD402_SIDE=0|1 (default 1)
"""
import csv
import glob
import os
import re
import sys

try:  # host console is cp936; CJK output must not abort the run
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = r"独立审计/证据"
CSVOUT = os.path.join(RAW, "AUD-402-常数台账.csv")
SIDEOUT = os.path.join(RAW, "AUD-402-常数台账.旁表-无名值与测试钉值.csv")
SCAN = os.path.join(RAW, "scan402")

HEADER = ["符号/键", "位置(路径:行)", "现行值", "单位", "坐标系/归一化", "精度要求",
          "有效有限域", "来源现状", "出处", "处置", "适用域", "备注"]
ALLOWED_SRC = ("有文献", "可公式导出", "需实验标定", "无来源", "不适用")
ALLOWED_DISPOSE = ("文献值", "公式导出", "实验标定", "待确认")
MECH = os.environ.get("AUD402_MECH", "1") != "0"
SIDE = os.environ.get("AUD402_SIDE", "1") != "0"

STRUCT = re.compile(r"(ABI|_abi|VERSION|version|struct_size|_COUNT$|_IDX|magic|MAGIC"
                    r"|_HEADER|_SIZE$|ORDINAL|SCHEMA|_N$|_NDX|slot|SLOT|_TAG|epoch)", re.I)
# a declaration comment that points at an authority document / DOI
POINTER = re.compile(r"(docs/|SCI-|ALG-|DISP-|\.md\b|DOI|arXiv|§|Fruchter|Iglewicz"
                     r"|LSST|Astropy|EBEQ|SR-?N|noalias|文献|论文|推导|导出式|标定|实测|裁决)")
FLOATY = re.compile(r"\d\.\d|\.\d+[eE]?|[eE][-+]?\d")


def load_authored():
    rows, errs = [], []
    for path in sorted(glob.glob(os.path.join(HERE, "rows_b*.tsv"))):
        for ln, line in enumerate(open(path, encoding="utf-8"), 1):
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            cells = line.split("\t")
            if len(cells) != 12:
                errs.append(f"{os.path.basename(path)}:{ln} 列数={len(cells)} 首格={cells[0][:40]}")
                continue
            if not any(cells[7].startswith(a) for a in ALLOWED_SRC):
                errs.append(f"{os.path.basename(path)}:{ln} 来源现状口径外: {cells[7][:40]}")
            if not any(cells[9].startswith(d) for d in ALLOWED_DISPOSE):
                errs.append(f"{os.path.basename(path)}:{ln} 处置口径外: {cells[9][:40]}")
            rows.append(cells)
    if errs:
        print("AUTHORED-ROW ERRORS:", len(errs))
        for e in errs[:20]:
            print("  ", e)
        sys.exit(6)
    return rows


def read_tsv(path):
    with open(path, encoding="utf-8") as fh:
        head = next(fh).rstrip("\n").split("\t")
        for line in fh:
            p = line.rstrip("\n").split("\t")
            if len(p) >= len(head):
                yield dict(zip(head, p))


def auto_row(rec, kind):
    """Named numeric constant -> ledger row, with whatever evidence the source line shows."""
    f, ln = rec["file"], rec["line"]
    name, val, text = rec["name"], rec.get("assigned") or rec.get("values"), rec["text"]
    struct = bool(STRUCT.search(name + " " + text))
    pointer = POINTER.search(text)
    prod = not (f.startswith("eng/tests/") or f.startswith("\u5b9e\u9a8c/"))
    if pointer:
        src = "有文献（声明行带权威指针，未逐条复核）"
        cite = text[pointer.start():pointer.start() + 120].strip()
        dispose = "文献值"
    elif struct:
        src = "不适用（结构性常数，非科学量）"
        cite, dispose = "", "待确认"
    else:
        src = "无来源（代码内字面量，未见文档/文献锚）"
        cite, dispose = "", "待确认"
    note = kind
    if not prod:
        note = "测试/实验钉值旁表：" + kind
    elif not pointer and not struct:
        note += "；未精读项，判读交 AUD-205 与四链路代理"
    return [name, f + ":" + ln, val, "未标注",
            "不适用（结构/实现常数）" if struct else "未标注",
            "不适用" if struct else "未标注", "未标注", src, cite, dispose,
            "生产代码" if prod else "测试/实验", note]


def main():
    authored = load_authored()
    named, side = [], []
    if MECH:
        for rec in read_tsv(os.path.join(SCAN, "named.tsv")):
            if not rec.get("name"):
                continue
            assigned = rec.get("assigned", "").strip()
            if not assigned:
                continue
            if not re.match(r"^[-+]?\d[\d_]*(?:\.\d[\d_]*)?(?:[eE][-+]?\d+)?[fFuUlL]*$",
                            assigned.replace(" ", "")):
                continue
            r = auto_row(rec, "命名常数旁表（纯数值初始化）")
            (named if r[10] == "生产代码" else side).append(r)
    if SIDE:
        for rec in read_tsv(os.path.join(SCAN, "cpp_hits.tsv")):
            if any(c in rec["class"] for c in ("define", "constexpr-named", "const-named")):
                continue
            txt = rec["text"]
            m = re.findall(r"[-+]?\d*\.\d+(?:[eE][-+]?\d+)?f?", txt)
            if not m:
                continue
            f = rec["file"]
            prod = not (f.startswith("eng/tests/") or f.startswith("\u5b9e\u9a8c/"))
            r = [rec.get("name") or "(无名)", f + ":" + rec["line"], ";".join(sorted(set(m))),
                 "未标注", "未标注", "未标注", "未标注",
                 "无来源（行内字面量，未见文档/文献锚）", "", "待确认",
                 "生产代码" if prod else "测试/实验",
                 "行内浮点字面量旁表" + ("" if prod else "（只登记不判对错）")]
            side.append(r)
    seen, rows = set(), []
    for r in authored + named + side:
        k = (r[0], r[1])
        if k in seen:
            continue
        seen.add(k)
        rows.append(r)
    with open(CSVOUT, "w", encoding="utf-8-sig", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(HEADER)
        w.writerows([r for r in rows])
    if SIDE:
        with open(SIDEOUT, "w", encoding="utf-8-sig", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(HEADER)
            w.writerows([r for r in side])
    print("authored:", len(authored), "| named-mech(in main):", len(named),
          "| sidecar rows:", len(side), "| main total:", len(rows))
    print("main:", CSVOUT)
    print("sidecar:", SIDEOUT)


if __name__ == "__main__":
    main()
