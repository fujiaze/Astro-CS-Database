#!/usr/bin/env python3
"""science_contract_lint: SCI 合同结构/claim ID/锚点检查 (SCI-001 引入)。
S1 必备章节齐备: 目的/符号/单位/坐标frame/有效域/连续定义/假设/不变量/极端/精度/专属问题/不可接受/Oracle/文献/Acceptance;
S2 claim ID 行格式 `> ID: SCI-<DOM>-NNN` 合法且唯一;
S3 文内引用的 lib/docs 路径存在; `path#NNN` / `path:NNN` 行号不超过文件长度。
用法: python3 tools/science_contract_lint.py docs/science/CALIBRATION.md [more...]  exit 0 = PASS。
"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SECTIONS_SCI = ["目的", "符号", "物理量和单位", "坐标 frame", "输入有效域", "连续定义", "假设",
            "不变量", "极端", "精度策略", "专属问题", "不可接受", "Oracle", "Primary literature", "Acceptance"]
SECTIONS_ALG = ["上游 SCI", "离散公式", "伪代码", "边界", "确定性与归约", "SIMD 安全",
                "复杂度", "Oracle", "容差", "关联"]
CLAIM_RE_SCI = re.compile(r"^>\s*ID:\s*(SCI-[A-Z0-9]{2,8}-\d{3})\s")
CLAIM_RE_ALG = re.compile(r"^>\s*ID:\s*(ALG-[A-Z0-9]{2,8}-\d{3})\s")
KINDS = {"sci": (SECTIONS_SCI, CLAIM_RE_SCI), "alg": (SECTIONS_ALG, CLAIM_RE_ALG)}

def check_file(path, errors, kind="sci"):
    rel = os.path.relpath(path, REPO)
    text = open(path, encoding="utf-8").read()
    lines = text.splitlines()
    sections, claim_re = KINDS[kind]
    for sec in sections:
        if not re.search(rf"^##+\s+.*{re.escape(sec)}", text, re.MULTILINE):
            errors.append(f"S1 {rel}: 缺章节 {sec}")
    ids = []
    for ln, line in enumerate(lines, 1):
        m = claim_re.match(line)
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
    args = sys.argv[1:]
    kind = "sci"
    if args and args[0] == "--kind":
        kind = args[1]; args = args[2:]
        if kind not in KINDS:
            print(f"unknown kind: {kind}"); return 2
    paths = args
    if not paths:
        print("usage: science_contract_lint.py <sci-doc.md> [...]")
        return 2
    for p in paths:
        if not os.path.isfile(p):
            errors.append(f"文件不存在: {p}")
            continue
        check_file(p, errors, kind)
    if errors:
        print(f"SCIENCE_CONTRACT_LINT_FAIL ({len(errors)}):")
        for e in errors[:20]:
            print(" ", e)
        return 1
    print(f"SCIENCE_CONTRACT_LINT_PASS kind={kind} files={len(paths)} sections={len(KINDS[kind][0])}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
