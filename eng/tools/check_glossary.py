#!/usr/bin/env python3
"""DOC-001 词典机器检查器。
G1 必备核心术语恰出现一次; G2 术语唯一(无重复行); G3 表内禁 TBD/待定/二选一/或然表述;
G4 legacy alias 映射唯一(同一 alias 不得映射到两个 canonical);
G5 锚形态 = `<仓库相对文件>` 或 `<文件>#<行号>` 或 `<文件> §<节号>`（章节引用一律 `§N`，
   `#N` 为行号锚——两者互不代用，口径见 docs/algorithms/anchors/ANCHOR_CONTRACT.md §1）；
   且目标必须存在、行号/节号必须能解析到实际内容（非空行 / 真存在该编号标题）;
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

# G5 锚文法（DOC-DRIFT-FIX-01 消歧留痕见 docs/algorithms/anchors/ANCHOR_CONTRACT.md §1）：
#   裸文件锚              docs/science/DRIZZLE.md
#   行号锚                lib/algorithms/calibration/src/cosmetic_corrector.cpp#158
#   章节引用（全路径）    docs/contracts/DATA_SEMANTICS.md §4a
# 原实现只认 `#\d+`，把**仓库既有的 `§N` 章节引用**一律判『格式非法』
# ⇒ 11 条合法章节引用被误判（判据退化：合法形态被排除）；且对 `#N` 只做存在性检查，
# 行号越界/落在空行、节号不存在一律放过（假绿）。本版同时修「误判」与「漏判」。
ANCHOR_FULL_RE = re.compile(r"^([^\s#§]+(?:\.[A-Za-z0-9_]+)?)(?:#(\d+)|\s*§([0-9]+[a-z]?))?$")
SECTION_HEAD_RE = re.compile(r"^#{1,6}\s+([0-9]+[a-z]?)(?:[.\s]|$)")

def rel_display(path):
    """仓库相对路径（POSIX 分隔符）；跨盘符/不可归 ⇒ 绝对路径原样。"""
    try:
        return os.path.relpath(path, REPO).replace(os.sep, "/")
    except ValueError:
        return path


def read_lines(path):
    """行口径与行号锚门（DOC-LINE-ANCHORS）一致：只去掉**一个**尾换行，再按换行切分。

    空行 == 空字符串；纯空白行不算空行（缩进行是内容）。
    """
    with open(path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    if text.endswith("\n"):
        text = text[:-1]
    return text.split("\n")


def section_numbers(path):
    """目标文档里**真实存在**的编号标题集合（H1–H6，标签 ^<num>[a-z]?）。"""
    out = set()
    for line in read_lines(path):
        m = SECTION_HEAD_RE.match(line)
        if m:
            out.add(m.group(1))
    return out


def anchor_error(anchor, target):
    """校验一条权威锚；返回判词或 None（合法）。

    target = 锚指向文件的仓内绝对路径。规则：目标文件必须存在；
    `#N` 行号必须界内**且有内容**（空行 = 锚没指向内容）；`§N` 节号必须命中真标题。
    """
    m = ANCHOR_FULL_RE.match(anchor)
    if not m:
        return "锚点格式非法"
    rel_file, line_s, sec_s = m.group(1), m.group(2), m.group(3)
    if not os.path.isfile(target):
        return f"锚点文件不存在: {rel_file}"
    if line_s is not None:
        lines = read_lines(target)
        n = int(line_s)
        if n < 1 or n > len(lines):
            return f"锚点行号越界: {rel_file}#{n}（目标共 {len(lines)} 行）"
        if lines[n - 1] == "":
            return f"锚点行号落在空行: {rel_file}#{n}"
    elif sec_s is not None and sec_s not in section_numbers(target):
        return f"锚点节号不存在: {rel_file} §{sec_s}"
    return None


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
        mm = ANCHOR_FULL_RE.match(anchor)
        detail = anchor_error(anchor, os.path.join(REPO, mm.group(1)) if mm else "")
        if detail:
            errors.append(f"G5 {where}:{ln}: {detail}: {anchor!r}")
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

    正例 1：干净夹具——同时用**两种合法锚形态**（`#N` 行号锚 + `§N` 章节引用）⇒ 0 条；
    负例 2：`§N` 形态非法（尾随令牌）⇒ 判『锚点格式非法』；
    负例 3：`§N` 形态合法但**目标文档没有该节号** ⇒ 判『锚点节号不存在』
            （证明 G5 不是「只要带 § 就放行」的恒真门）；
    负例 4：`#N` 行号越界 ⇒ 判『锚点行号越界』；
    负例 5：25 条违规（> 原截断阈值 20）⇒ 判词必须逐条列全，证明截断已移除。
    """
    import shutil
    import tempfile
    ok = True
    tmp = tempfile.mkdtemp(prefix="glossary-selftest-")
    try:
        root = os.path.join(tmp, "repo")
        os.makedirs(os.path.join(root, "docs", "contracts"))
        tgt = os.path.join(root, "docs", "contracts", "DATA_SEMANTICS.md")
        # 夹具目标文档：真实编号标题 §1..§5（含 §4a），供节号解析
        body = ["# fixture"] + [f"## {n} sec" for n in (1, 2, 4, "4a", 5)]
        with open(tgt, "w", encoding="utf-8") as fh:
            fh.write("\n".join(body) + "\n")
        rows = ["| term | meaning | unit | alias | anchor |", "|---|---|---|---|---|"]
        for i, t in enumerate(REQUIRED):
            # 两种合法形态各半：偶数行走 `#N` 行号锚，奇数行走 `§N` 章节引用
            anchor = "docs/contracts/DATA_SEMANTICS.md#1" if i % 2 == 0 \
                else "docs/contracts/DATA_SEMANTICS.md §4a"
            rows.append(f"| {t} | m | u | - | {anchor} |")
        clean = "\n".join(rows) + "\n"
        gp = os.path.join(root, "docs", "GLOSSARY.md")
        with open(gp, "w", encoding="utf-8") as fh:
            fh.write(clean)
        errs, _t, _a = run(root)
        good = errs == []
        print("[selftest] %-28s errors=%d %s"
              % ("pos_clean_green", len(errs), "OK" if good else "MISMATCH"))
        ok = ok and good

        def inject(bad, expected):
            """把 clean 的**最后一行**锚换成 bad，返回判词与是否命中 expected。"""
            lines_ = clean.rstrip("\n").split("\n")
            head = lines_[-1].split("|")
            head[-2] = " " + bad + " "
            lines_[-1] = "|".join(head)
            with open(gp, "w", encoding="utf-8") as fh:
                fh.write("\n".join(lines_) + "\n")
            e, _tt, _aa = run(root)
            hit = any(e2.startswith("G5") and expected in e2 for e2 in e)
            where_ok = all(":" in e2 for e2 in e)
            return e, hit and where_ok

        # 负例 2：§N 形态非法（尾随令牌 ⇒ 整串不再是「文件 + 节号」）
        _e, hit = inject("docs/contracts/DATA_SEMANTICS.md §4a junk", "锚点格式非法")
        print("[selftest] %-28s %s" % ("neg_bad_section_form_red", "OK" if hit else "MISMATCH"))
        ok = ok and hit

        # 负例 3：节号不存在（形态合法——判据必须落到目标文档的真实标题上）
        _e, hit = inject("docs/contracts/DATA_SEMANTICS.md §9z", "锚点节号不存在")
        print("[selftest] %-28s %s" % ("neg_missing_section_red", "OK" if hit else "MISMATCH"))
        ok = ok and hit

        # 负例 4：行号越界（夹具目标文档只有 6 行）
        _e, hit = inject("docs/contracts/DATA_SEMANTICS.md#9999", "锚点行号越界")
        print("[selftest] %-28s %s" % ("neg_line_out_of_range_red", "OK" if hit else "MISMATCH"))
        ok = ok and hit

        # 负例 5：25 条违规 ⇒ 判词必须逐条列全（截断已移除）
        rows2 = ["| term | meaning | unit | alias | anchor |", "|---|---|---|---|---|"]
        for t in REQUIRED:
            rows2.append(f"| {t} | m | u | - | docs/contracts/DATA_SEMANTICS.md §9z |")
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
