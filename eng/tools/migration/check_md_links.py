#!/usr/bin/env python3
"""check_md_links.py — markdown 相对链接目标审计（迁移后断裂检查）
用法: python3 eng/tools/migration/check_md_links.py [root]  默认仓库根
扫描 *.md 的 [text](target) 相对链接，目标不存在即列出。http(s)/锚点/纯锚跳过。
"""
import os, re, sys
REPO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SKIP = {".git", "build", "run", "gaia", "testdata", "node_modules", "__pycache__"}
LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
bad = total = 0
for root, dirs, files in os.walk(REPO):
    dirs[:] = [d for d in dirs if d not in SKIP]
    for f in files:
        if not f.endswith(".md"):
            continue
        p = os.path.join(root, f)
        rel = os.path.relpath(p, REPO)
        try:
            text = open(p, encoding="utf-8").read()
        except (UnicodeDecodeError, OSError):
            continue
        for m in LINK.finditer(text):
            tgt = m.group(1)
            if tgt.startswith(("http://", "https://", "#", "mailto:")):
                continue
            tgt = tgt.split("#")[0]
            if not tgt:
                continue
            total += 1
            ap = os.path.normpath(os.path.join(os.path.dirname(p), tgt))
            if not os.path.exists(ap):
                bad += 1
                print(f"[断链] {rel} -> {tgt}")
print(f"== 链接 {total}，断链 {bad}")
sys.exit(0)
