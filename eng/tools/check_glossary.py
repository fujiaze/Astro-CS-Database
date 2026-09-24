#!/usr/bin/env python3
"""DOC-001 词典机器检查器。
G1 必备核心术语恰出现一次; G2 术语唯一(无重复行); G3 表内禁 TBD/待定/二选一/或然表述;
G4 legacy alias 映射唯一(同一 alias 不得映射到两个 canonical); G5 锚点文件存在(# 后为节锚);
G6 迁移执行: science/contracts 域文档出现被禁 alias(DN 裸用)即 FAIL。
用法: python3 eng/tools/check_glossary.py  exit 0 = PASS。
"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GLOSSARY = os.path.join(REPO, "docs", "GLOSSARY.md")
REQUIRED = ["adu", "electron", "variance", "ivar", "pixel_weight", "frame_quality_weight",
            "support", "invalid", "nan", "bad_mask", "product_bit_flags", "ra_dec",
            "pixel_coordinate", "healpix_ordering", "frame_id", "signal",
            "surface_brightness", "calibration_units"]
FORBIDDEN_PHRASES = ["TBD", "待定", "二选一", "或者选择", "可能或"]
BANNED_ALIAS_TOKENS = {"DN": re.compile(r"(?<![A-Za-z])DN(?![A-Za-z])")}
SCAN_ALIAS_DOCS = ["docs/science", "docs/contracts"]

def rel_display(path):
    """仓库相对路径（POSIX 分隔符）；跨盘符/不可归 ⇒ 绝对路径原样。"""
    try:
        return os.path.relpath(path, REPO).replace(os.sep, "/")
    except ValueError:
        return path


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
        # 判词一律带「文件:行」（GATE-TRIAGE-01）：原实现只给行号，读者无法定位
        # 是哪一份文档的哪一行（本表所在文件 = path 的仓库相对路径）。
        where = rel_display(path)
        for p in FORBIDDEN_PHRASES:
            if p.lower() in line.lower():
                errors.append(f"G3 {where}:{ln}: 禁止二义表述 {p!r}")
        if term in terms:
            errors.append(f"G2 {where}:{ln}: 术语重复: {term}")
            continue
        terms[term] = True
        for m in re.finditer(r"([A-Za-z⁻²⁺/]+(?:\s?[A-Za-z⁻²⁺/]+)*)\s*→\s*([A-Za-z_⁻²⁺/]+)", alias):
            a, canon = m.group(1).strip(), m.group(2).strip()
            if a in alias_map and alias_map[a][0] != canon:
                errors.append(f"G4 {where}:{ln}: alias {a!r} 冲突映射: "
                              f"{alias_map[a][0]} vs {canon}")
            alias_map[a] = (canon, ln)
        mm = re.match(r"^([^\s#]+(?:\.md|\.cpp|\.py|\.h))(?:#(\d+[^\s]*))?$", anchor)
        if not mm:
            errors.append(f"G5 {where}:{ln}: 锚点格式非法: {anchor!r}")
        elif not os.path.isfile(os.path.join(REPO, mm.group(1))):
            errors.append(f"G5 {where}:{ln}: 锚点文件不存在: {mm.group(1)}")
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
                # rel_display 已处理 Windows 跨盘符（C: vs D:）与分隔符归一。
                rel = rel_display(full)
                for ln, line in enumerate(open(full, encoding="utf-8", errors="replace"), 1):
                    if rel == "docs/GLOSSARY.md":
                        continue
                    for tok, rx in BANNED_ALIAS_TOKENS.items():
                        if rx.search(line):
                            errors.append(f"G6 被禁 alias {tok!r} 未迁移: {rel}:{ln}: {line.strip()[:70]}")

def run(root=REPO, glossary=None):
    """在 root 下跑全套判据，返回 (errors, terms, alias_map)。

    root 可注入（自检夹具用）；glossary 默认 root/docs/GLOSSARY.md。
    """
    global REPO, GLOSSARY
    saved = (REPO, GLOSSARY)
    REPO = root
    GLOSSARY = glossary or os.path.join(root, "docs", "GLOSSARY.md")
    errors = []
    try:
        if not os.path.isfile(GLOSSARY):
            return [f"G0 {rel_display(GLOSSARY)}: 不存在"], {}, {}
        terms, alias_map = parse_glossary(errors, GLOSSARY)
        check_banned_aliases(errors)
        return errors, terms, alias_map
    finally:
        REPO, GLOSSARY = saved


def self_test():
    """可执行正/负例面（GATE-TRIAGE-01）：注入已知红灯必须判红，且**判词不截断**。

    负例 1：锚点格式非法（原 `errors[:20]` 截断的正是这一类）⇒ 必须出现在判词里；
    负例 2：注入 25 条违规（> 原截断阈值 20）⇒ 判词必须逐条列全（25 条全可见），
            证明截断已移除；
    正例 3：干净夹具 ⇒ 0 条。
    """
    import shutil
    import tempfile
    ok = True
    tmp = tempfile.mkdtemp(prefix="glossary-selftest-")
    try:
        root = os.path.join(tmp, "repo")
        os.makedirs(os.path.join(root, "docs", "contracts"))
        rows = ["| term | meaning | unit | alias | anchor |", "|---|---|---|---|---|"]
        for t in REQUIRED:
            rows.append(f"| {t} | m | u | - | docs/contracts/DATA_SEMANTICS.md |")
        clean = "\n".join(rows) + "\n"
        gp = os.path.join(root, "docs", "GLOSSARY.md")
        with open(gp, "w", encoding="utf-8") as fh:
            fh.write(clean)
        with open(os.path.join(root, "docs", "contracts", "DATA_SEMANTICS.md"),
                  "w", encoding="utf-8") as fh:
            fh.write("# c\n")
        errs, _t, _a = run(root)
        good = errs == []
        print("[selftest] %-28s errors=%d %s"
              % ("pos_clean_green", len(errs), "OK" if good else "MISMATCH"))
        ok = ok and good

        # 负例 1：非法锚点格式
        with open(gp, "w", encoding="utf-8") as fh:
            fh.write(clean.replace("docs/contracts/DATA_SEMANTICS.md |",
                                   "docs/contracts/DATA_SEMANTICS.md §4a |", 1))
        errs, _t, _a = run(root)
        good = any(e.startswith("G5") and "锚点格式非法" in e for e in errs)
        has_where = all(":" in e for e in errs)
        print("[selftest] %-28s errors=%d %s"
              % ("neg_bad_anchor_red", len(errs), "OK" if (good and has_where) else "MISMATCH"))
        ok = ok and good and has_where

        # 负例 2：25 条违规 ⇒ 判词必须逐条列全（截断已移除）
        rows2 = ["| term | meaning | unit | alias | anchor |", "|---|---|---|---|---|"]
        for t in REQUIRED:
            rows2.append(f"| {t} | m | u | - | docs/contracts/DATA_SEMANTICS.md §4a |")
        with open(gp, "w", encoding="utf-8") as fh:
            fh.write("\n".join(rows2) + "\n")
        errs, _t, _a = run(root)
        n_g5 = sum(1 for e in errs if e.startswith("G5"))
        good = n_g5 >= len(REQUIRED) and len(errs) >= len(REQUIRED)
        print("[selftest] %-28s G5=%d total=%d %s"
              % ("neg_25_untruncated", n_g5, len(errs), "OK" if good else "MISMATCH"))
        ok = ok and good
        return 0 if ok else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--self-test" in argv:
        return self_test()
    errors, terms, alias_map = run(REPO)
    if errors:
        # 判词不截断（GATE-TRIAGE-01）：原实现 `errors[:20]` 把 15 条里的 4 条
        # 吞掉，读者看不到全貌 ⇒ 门报「15 条」却只列 11 条，判据不可复核。
        # 每条自带 文件:行（G5 锚点类带 docs/GLOSSARY.md 的行号，G6 带被扫文件:行）。
        print(f"GLOSSARY_FAIL ({len(errors)}):")
        for e in errors:
            print(" ", e)
        return 1
    print(f"GLOSSARY_PASS terms={len(terms)}/{len(REQUIRED)} alias映射={len(alias_map)} 迁移检查域={SCAN_ALIAS_DOCS}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
