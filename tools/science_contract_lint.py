#!/usr/bin/env python3
"""science_contract_lint: SCI 合同结构/claim ID/锚点检查 (SCI-001 引入)。
S1 必备章节齐备: 目的/符号/单位/坐标frame/有效域/连续定义/假设/不变量/极端/精度/专属问题/不可接受/Oracle/文献/Acceptance;
S2 claim ID 行格式 `> ID: SCI-<DOM>-NNN` 合法且唯一;
S3 文内引用的 lib/docs 路径存在; `path#NNN` / `path:NNN` 行号不超过文件长度。
用法: python3 tools/science_contract_lint.py docs/science/CALIBRATION.md [more...]  exit 0 = PASS。
"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SECTIONS = ["目的", "符号", "物理量和单位", "坐标 frame", "输入有效域", "连续定义", "假设",
            "不变量", "极端", "精度策略", "专属问题", "不可接受", "Oracle", "Primary literature", "Acceptance"]
CLAIM_RE = re.compile(r"^>\s*ID:\s*(SCI-[A-Z0-9]{2,8}-\d{3})\s")

def check_file(path, errors):
    rel = os.path.relpath(path, REPO)
    text = open(path, encoding="utf-8").read()
    lines = text.splitlines()
    for sec in SECTIONS:
        if not re.search(rf"^##+\s+.*{re.escape(sec)}", text, re.MULTILINE):
            errors.append(f"S1 {rel}: 缺章节 {sec}")
    ids = []
    for ln, line in enumerate(lines, 1):
        m = CLAIM_RE.match(line)
        if m:
            ids.append(m.group(1))
        for mp in re.finditer(r"`((?:lib|docs|tools)/[\w/.\-]+\.(?:cpp|h|md|py))(?::(\d+)|#(\d+))?", line):
            fpath, l1, l2 = mp.group(1), mp.group(2), mp.group(3)
            full = os.path.join(REPO, fpath)
            if not os.path.isfile(full):
                errors.append(f"S3 {rel}:{ln}: 引用文件不存在: {fpath}")
                continue
            n = int(l1 or l2 or 0)
            if n and n > sum(1 for _ in open(full, errors="replace")):
                errors.append(f"S3 {rel}:{ln}: 行号越界 {fpath}:{n}")
    if not ids:
        errors.append(f"S2 {rel}: 无 claim ID 行(> ID: SCI-...)")
    if len(ids) != len(set(ids)):
        errors.append(f"S2 {rel}: claim ID 重复")

def main():
    errors = []
    paths = sys.argv[1:]
    if not paths:
        print("usage: science_contract_lint.py <sci-doc.md> [...]")
        return 2
    for p in paths:
        if not os.path.isfile(p):
            errors.append(f"文件不存在: {p}")
            continue
        check_file(p, errors)
    if errors:
        print(f"SCIENCE_CONTRACT_LINT_FAIL ({len(errors)}):")
        for e in errors[:20]:
            print(" ", e)
        return 1
    print(f"SCIENCE_CONTRACT_LINT_PASS files={len(paths)} sections={len(SECTIONS)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
