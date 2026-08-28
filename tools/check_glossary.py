#!/usr/bin/env python3
"""DOC-001 词典机器检查器。
G1 必备核心术语恰出现一次; G2 术语唯一(无重复行); G3 表内禁 TBD/待定/二选一/或然表述;
G4 legacy alias 映射唯一(同一 alias 不得映射到两个 canonical); G5 锚点文件存在(# 后为节锚);
G6 迁移执行: science/contracts 域文档出现被禁 alias(DN 裸用)即 FAIL。
用法: python3 tools/check_glossary.py  exit 0 = PASS。
"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GLOSSARY = os.path.join(REPO, "docs", "GLOSSARY.md")
REQUIRED = ["adu", "electron", "variance", "ivar", "pixel_weight", "frame_quality_weight",
            "support", "invalid", "nan", "bad_mask", "product_bit_flags", "ra_dec",
            "pixel_coordinate", "healpix_ordering", "frame_id", "signal",
            "surface_brightness", "calibration_units"]
FORBIDDEN_PHRASES = ["TBD", "待定", "二选一", "或者选择", "可能或"]
BANNED_ALIAS_TOKENS = {"DN": re.compile(r"(?<![A-Za-z])DN(?![A-Za-z])")}
SCAN_ALIAS_DOCS = ["docs/science", "docs/contracts"]

def parse_glossary(errors, path=GLOSSARY):
    terms, alias_map = {}, {}
    in_table = False
    for ln, line in enumerate(open(path, encoding="utf-8"), 1):
        if line.startswith("| term |"):
            in_table = True
            continue
        if not (in_table and line.startswith("|")):
            if in_table and not line.startswith("|"):
                in_table = False
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) != 5 or set(cells[0]) <= {"-", " ", "term"}:
            continue  # 表头/分隔行
        term, meaning, unit, alias, anchor = cells
        for p in FORBIDDEN_PHRASES:
            if p.lower() in line.lower():
                errors.append(f"G3 {term}:{ln}: 禁止二义表述 {p!r}")
        if term in terms:
            errors.append(f"G2 {ln}: 术语重复: {term}")
            continue
        terms[term] = True
        for m in re.finditer(r"([A-Za-z⁻²⁺/]+(?:\s?[A-Za-z⁻²⁺/]+)*)\s*→\s*([A-Za-z_⁻²⁺/]+)", alias):
            a, canon = m.group(1).strip(), m.group(2).strip()
            if a in alias_map and alias_map[a][0] != canon:
                errors.append(f"G4 {ln}: alias {a!r} 冲突映射: {alias_map[a][0]} vs {canon}")
            alias_map[a] = (canon, ln)
        mm = re.match(r"^([^\s#]+(?:\.md|\.cpp|\.py|\.h))(?:#(\d+[^\s]*))?$", anchor)
        if not mm:
            errors.append(f"G5 {ln}: 锚点格式非法: {anchor!r}")
        elif not os.path.isfile(os.path.join(REPO, mm.group(1))):
            errors.append(f"G5 {ln}: 锚点文件不存在: {mm.group(1)}")
    for t in REQUIRED:
        if t not in terms:
            errors.append(f"G1 缺失必备术语: {t}")
    return terms, alias_map

def check_banned_aliases(errors, roots=None):
    roots = roots or SCAN_ALIAS_DOCS
    for root in roots:
        for dirpath, _, files in os.walk(os.path.join(REPO, root)):
            for fn in files:
                if not fn.endswith(".md"):
                    continue
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, REPO)
                for ln, line in enumerate(open(full, encoding="utf-8", errors="replace"), 1):
                    if rel == "docs/GLOSSARY.md":
                        continue
                    for tok, rx in BANNED_ALIAS_TOKENS.items():
                        if rx.search(line):
                            errors.append(f"G6 被禁 alias {tok!r} 未迁移: {rel}:{ln}: {line.strip()[:70]}")

def main():
    errors = []
    if not os.path.isfile(GLOSSARY):
        print("GLOSSARY_FAIL: docs/GLOSSARY.md 不存在")
        return 1
    terms, alias_map = parse_glossary(errors)
    check_banned_aliases(errors)
    if errors:
        print(f"GLOSSARY_FAIL ({len(errors)}):")
        for e in errors[:20]:
            print(" ", e)
        return 1
    print(f"GLOSSARY_PASS terms={len(terms)}/{len(REQUIRED)} alias映射={len(alias_map)} 迁移检查域={SCAN_ALIAS_DOCS}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
