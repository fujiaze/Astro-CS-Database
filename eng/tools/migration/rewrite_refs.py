#!/usr/bin/env python3
"""rewrite_refs.py — DOC-REORG 批次引用重写（机械、可审计）
用法: python3 eng/tools/migration/rewrite_refs.py --batch design-root [--apply]
默认 dry-run 打印将改动的 文件/处数/样例。只做路径级重写，不碰正文语义。
"""
import argparse, os, re, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SKIP_DIRS = {".git", "build", "run", "gaia", "testdata", "node_modules", "artifacts"}
TEXT_EXT = {".md", ".cpp", ".h", ".c", ".cc", ".hpp", ".py", ".json", ".txt", ".cmake", ".yaml", ".yml", ".sh", ".csv", ".tsv"}

def iter_files():
    for root, dirs, files in os.walk(REPO):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in files:
            p = os.path.join(root, f)
            ext = os.path.splitext(f)[1]
            if ext in TEXT_EXT or f == "CMakeLists.txt" or f == "DOCUMENT_INDEX.yaml":
                yield p

def rule_design_root(rel, text):
    """批次1: docs/ASTROCS_DESIGN.md 裸名 → docs 外加 docs/ 前缀；docs 内不变；防双前缀。"""
    n = 0
    def out_prefix(mm):
        nonlocal n; n += 1
        return mm.group(1) + "docs/ASTROCS_DESIGN.md"
    if rel.startswith("docs" + os.sep) or rel.startswith("docs/"):
        if rel.replace(os.sep, "/") == "docs/DOCUMENT_INDEX.yaml":
            pass  # INDEX 的 path 字段是仓库根相对路径，按 docs 外处理
        else:
            return text, 0  # docs 内保持裸名（同目录）
    # 防止已有 docs/ASTROCS_DESIGN.md 被二次前缀
    t2 = re.sub(r"(?<!docs/)(?<!\u0064ocs/)\bASTROCS_DESIGN\.md\b", "docs/ASTROCS_DESIGN.md", text)
    n = t2.count("docs/ASTROCS_DESIGN.md") - text.count("docs/ASTROCS_DESIGN.md")
    # docs/ASTROCS_DESIGN（无 .md 后缀的行文引用，如 "docs/ASTROCS_DESIGN §9"）——只在代码注释/行文中，同样加前缀
    t3 = re.sub(r"(?<!docs/)(?<![A-Za-z_])docs/ASTROCS_DESIGN(?=\s*[§\s:,)（(、])(?!\.md)", "docs/ASTROCS_DESIGN", t2)
    n += t3.count("docs/ASTROCS_DESIGN") - t2.count("docs/ASTROCS_DESIGN")
    return t3, n

BATCHES = {"design-root": [rule_design_root]}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--batch", required=True, choices=sorted(BATCHES))
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    total_f = total_n = 0
    for p in iter_files():
        rel = os.path.relpath(p, REPO)
        try:
            text = open(p, encoding="utf-8").read()
        except (UnicodeDecodeError, OSError):
            continue
        new = text
        for fn in BATCHES[a.batch]:
            new, _ = fn(rel, new)
        if new != text:
            total_f += 1; total_n += new.count("docs/ASTROCS_DESIGN") - text.count("docs/ASTROCS_DESIGN")
            print(f"[{rel}] +{new.count('docs/ASTROCS_DESIGN')-text.count('docs/ASTROCS_DESIGN')}")
            if a.apply:
                open(p, "w", encoding="utf-8").write(new)
    print(f"== {'APPLIED' if a.apply else 'DRY-RUN'}: {total_f} files, {total_n} replacements")
    return 0

if __name__ == "__main__":
    sys.exit(main())
