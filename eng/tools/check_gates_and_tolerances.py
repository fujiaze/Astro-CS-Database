#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F-2 机器门：冻结门表 docs/algorithms/GATES_AND_TOLERANCES.md 可校验性。

背景（SCI-FIX-PSF 第 9 项 / R-3 §5.6）：本域原有 7 条门「零标定依据」，
2/5 条连量测域都没写、无一条有阈值来源字段。本检查器把「门表」变成**机器事实源**：

规则（exit 0 = PASS）：
  G1 表结构：§3 冻结门表存在，表头前 8 列名固定（门ID/判据式/量测域/统计量/
     SNR/信噪定义/阈值/阈值来源/证据ID），可选第 9 列「发布门」；
  G2 门ID：形如 G-P1-<大写/数字/->、全表唯一、每行非空；
  G3 统计量显式（R4）：必须含 max/median/p95/rms/bitwise/精确 之一；
  G4 量测域显式（R3）：必须含域关键词之一（合成/图像块/真实帧/产品级/
     契约函数级/导出边界/独立密集域/任意帧/FP64 通道/FP32 通道）；
     且「量测域」不得以拟合采样网格作检验域（出现 7×7 网格 而无 ≥/独立 ⇒ 红）；
  G5 证据（R2）：发布门=Y ⇒ 证据ID 不得为 UNJUSTIFIED；`ctest:<name>` 必须是仓内
     CMakeLists 里真实存在的 add_test 名；路径型证据必须真实存在（run/** 允许）；
     UNJUSTIFIED 只允许出现在 发布门=N 的行；
  G6 登记：本表必须在 docs/DOCUMENT_INDEX.yaml 活动区登记（GOV-002 规则 6）。

用法：
  python3 eng/tools/check_gates_and_tolerances.py            # 校验真表
  python3 eng/tools/check_gates_and_tolerances.py --self-test # 负例注入必红（能红能绿）
"""
from __future__ import annotations

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TABLE_DOC = "docs/algorithms/GATES_AND_TOLERANCES.md"
INDEX_DOC = "docs/DOCUMENT_INDEX.yaml"

REQUIRED_COLS = ["门ID", "判据式", "量测域", "统计量", "SNR/信噪定义", "阈值", "阈值来源", "证据ID"]
STAT_TOKENS = ["max", "median", "p95", "rms", "bitwise", "精确", "比例", "计数", "密度"]
DOMAIN_TOKENS = [
    "合成", "图像块", "真实帧", "产品级", "契约函数级", "导出边界",
    "独立密集域", "任意帧", "FP64 通道", "FP32 通道",
]
GATE_ID_RE = re.compile(r"^G-P1-[A-Z0-9-]+$")


def split_row(line: str) -> list[str]:
    r"""按未转义的 | 切分 Markdown 表格行（单元格内 \| 是转义竖线）。"""
    cells, cur, esc = [], [], False
    for ch in line.strip():
        if esc:
            cur.append(ch)
            esc = False
            continue
        if ch == "\\":
            cur.append(ch)
            esc = True
            continue
        if ch == "|":
            cells.append("".join(cur))
            cur = []
            continue
        cur.append(ch)
    cells.append("".join(cur))
    # 去掉首尾竖线产生的空单元格
    if cells and cells[0].strip() == "":
        cells = cells[1:]
    if cells and cells[-1].strip() == "":
        cells = cells[:-1]
    return [c.strip() for c in cells]


def parse_table(text: str) -> tuple[list[str], list[list[str]]]:
    lines = text.splitlines()
    start = None
    for i, ln in enumerate(lines):
        if ln.startswith("## 3"):
            start = i
            break
    if start is None:
        return [], []
    header: list[str] = []
    rows: list[list[str]] = []
    for ln in lines[start:]:
        if not ln.strip().startswith("|"):
            if header and rows:
                break
            continue
        cells = split_row(ln)
        if not header:
            header = cells
            continue
        if all(set(c) <= set("-: ") for c in cells):
            continue  # 分隔行
        rows.append(cells)
    return header, rows


def collect_ctest_names() -> set[str]:
    names: set[str] = set()
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [
            d for d in dirnames
            if d not in (".git", "build", "run", "third_party", "__pycache__", "node_modules")
        ]
        for fn in filenames:
            if fn != "CMakeLists.txt":
                continue
            try:
                with open(os.path.join(dirpath, fn), encoding="utf-8", errors="ignore") as f:
                    for m in re.finditer(r"add_test\(\s*NAME\s+([A-Za-z0-9_.\-]+)", f.read()):
                        names.add(m.group(1))
            except OSError:
                continue
    return names


def check_evidence(token: str, ctest_names: set[str], release: bool) -> tuple[bool, str]:
    tok = token.strip()
    if not tok:
        return False, "证据ID 为空"
    if tok.upper().startswith("UNJUSTIFIED"):
        if release:
            return False, "发布门=Y 但证据 UNJUSTIFIED (R2)"
        return True, "非发布门 UNJUSTIFIED"
    if tok.startswith("ctest:"):
        name = tok[len("ctest:"):].strip()
        if name not in ctest_names:
            return False, f"ctest 目标不存在于任何 CMakeLists: {name}"
        return True, "ctest 目标存在"
    # 路径型证据（允许 '路径（说明）' 与 '路径::symbol' 形式）
    path = re.split(r"[（(]", tok)[0].split("::")[0].strip()
    if not path or not re.match(r"^[A-Za-z0-9_./-]+$", path):
        return True, "非路径型证据文本（跳过存在性检查）"
    if os.path.exists(os.path.join(ROOT, path)):
        return True, "路径存在"
    return False, f"证据路径不存在: {path}"


def validate(text: str, index_text: str, ctest_names: set[str]) -> tuple[list[str], list[str]]:
    """返回 (failures, passes)。"""
    fails: list[str] = []
    passes: list[str] = []
    header, rows = parse_table(text)
    if not header:
        return ["TABLE §3 冻结门表未找到"], []
    if header[: len(REQUIRED_COLS)] != REQUIRED_COLS:
        fails.append(f"G1 表头不符: 期望前 8 列 {REQUIRED_COLS}, 实得 {header}")
    else:
        passes.append("G1 表头 8 列固定")
    if len(header) < 9 or header[8] != "发布门":
        fails.append("G1 缺第 9 列「发布门」（R2 需要 => Y/N）")
    if len(rows) < 10:
        fails.append(f"G1 行数异常: {len(rows)} (<10, 疑表解析失败)")

    seen: set[str] = set()
    for r in rows:
        gid = r[0] if r else ""
        if not GATE_ID_RE.match(gid):
            fails.append(f"G2 门ID 非法: {gid!r}")
            continue
        if gid in seen:
            fails.append(f"G2 门ID 重复: {gid}")
        seen.add(gid)
        if len(r) < 9 or any(not c.strip() for c in r[:9]):
            fails.append(f"G2 {gid} 存在空单元格")
            continue
        stat = r[3]
        if not any(t in stat for t in STAT_TOKENS):
            fails.append(f"G3 {gid} 统计量不显式: {stat!r}")
        domain = r[2]
        if not any(t in domain for t in DOMAIN_TOKENS):
            fails.append(f"G4 {gid} 量测域缺域关键词: {domain!r}")
        if "7×7" in domain and "≥" not in domain and "独立" not in domain:
            fails.append(f"G4 {gid} 量测域疑似自证门 (7×7 拟合域作检验域)")
        release = r[8].strip().upper() == "Y"
        if r[8].strip().upper() not in ("Y", "N"):
            fails.append(f"G5 {gid} 发布门列非 Y/N: {r[8]!r}")
        for tok in re.split(r"[;；]", r[7]):
            ok, why = check_evidence(tok, ctest_names, release)
            if not ok:
                fails.append(f"G5 {gid} {why}")
        if r[6].strip().upper().startswith("UNJUSTIFIED") and release:
            fails.append(f"G5 {gid} 发布门=Y 但阈值来源 UNJUSTIFIED")
        if not any(f.startswith(("G2", "G3", "G4", "G5")) and gid in f for f in fails):
            passes.append(f"G2-G5 {gid} OK")

    if TABLE_DOC not in index_text:
        fails.append(f"G6 {TABLE_DOC} 未在 {INDEX_DOC} 登记 (GOV-002 规则 6)")
    else:
        passes.append(f"G6 {TABLE_DOC} 已登记文档索引")
    return fails, passes


def read(path: str) -> str:
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def self_test(text: str, index_text: str, ctest_names: set[str]) -> int:
    """负例注入: 每条规则至少一个必红用例（能红能绿, ENGINEERING_SPEC §8）。"""
    cases: list[tuple[str, str]] = []
    cases.append(("删一列(表头缺 证据ID)", text.replace("| 证据ID | 发布门 |", "| 发布门 |", 1)))
    cases.append(("发布门=Y + UNJUSTIFIED 证据",
                  text.replace("| ctest:p1psf_centroid_gate | Y |", "| UNJUSTIFIED | Y |", 1)))
    cases.append(("ctest 目标不存在",
                  text.replace("| ctest:p1psf_centroid_gate | Y |", "| ctest:no_such_test_xyz | Y |", 1)))
    cases.append(("门ID 重复",
                  text.replace("| G-P1-CENTROID-U16 |", "| G-P1-CENTROID-SCI |", 1)))
    cases.append(("统计量不显式",
                  text.replace("| max | SNR_peak（检测侧） | 0.3 px |", "| 误差小 | SNR_peak（检测侧） | 0.3 px |", 1)))
    cases.append(("自证门量测域",
                  text.replace("| **独立密集域**：中心 90%", "| 7×7 网格：中心 90%", 1)))
    bad = 0
    for name, mutated in cases:
        fails, _ = validate(mutated, index_text, ctest_names)
        detected = len(fails) > 0
        print(f"  [{'PASS' if detected else 'FAIL'}] 注入必红: {name}"
              + ("" if detected else f"  (未被检出)"))
        if not detected:
            bad += 1
    # 文档索引缺失注入
    fails, _ = validate(text, "", ctest_names)
    detected = any("G6" in f for f in fails)
    print(f"  [{'PASS' if detected else 'FAIL'}] 注入必红: 文档索引未登记")
    if not detected:
        bad += 1
    print(f"SELF-TEST {'PASS' if bad == 0 else 'FAIL'} ({len(cases) + 1} 注入, {bad} 未检出)")
    return 0 if bad == 0 else 1


def main() -> int:
    text = read(TABLE_DOC)
    index_text = read(INDEX_DOC)
    ctest_names = collect_ctest_names()
    if "--self-test" in sys.argv:
        return self_test(text, index_text, ctest_names)
    fails, passes = validate(text, index_text, ctest_names)
    for p in passes:
        print(f"PASS {p}")
    for f in fails:
        print(f"FAIL {f}")
    header, rows = parse_table(text)
    print(f"GATES TABLE GATE {'PASS' if not fails else 'FAIL'} ({len(rows)} gates, "
          f"{len(passes)} pass, {len(fails)} fail)")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
