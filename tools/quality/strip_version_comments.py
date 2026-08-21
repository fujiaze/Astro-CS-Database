#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""strip_version_comments.py — 迁移开发轮次历史注释（Vxx/Rxx/控制包/审计）。

COMMENT_STANDARD：production 注释保留 WHY/SCIENCE/INVARIANT，轮次历史进
git/CHANGELOG。本脚本机械剥离轮次标记，保留科学语义；应用后必须
重编译全量 + 重跑测试 + check_comment_hygiene 复扫。

仅处理 lib/ 下 first-party C/C++ 源（排除 build/archive/third_party）。
"""

from __future__ import annotations

import os
import re
import subprocess
import sys


def _deduce_root() -> str:
    # auto-deduce project root: walk up until docs/ and lib/ found (Linux-portable)
    try:
        p = os.path.abspath(__file__)
        cur = os.path.dirname(p)
        for _ in range(5):
            if os.path.isdir(os.path.join(cur, "docs")) and os.path.isdir(os.path.join(cur, "lib")):
                return cur
            parent = os.path.dirname(cur)
            if parent == cur:
                break
            cur = parent
    except Exception:
        pass
    cwd = os.getcwd()
    if os.path.isdir(os.path.join(cwd, "docs")) and os.path.isdir(os.path.join(cwd, "lib")):
        return cwd
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ROOT = _deduce_root()

# 1) 注释开头轮次标记 + 可选括号 + 冒号/空格：
#    "// V14 (G3)：..." "// R1（V4）：..." "// V4.17 修复: ..."
PREFIX = re.compile(
    r"^(\s*//+\s*)"
    r"(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+)?|R\d+|MICROFIX(?:\s*#\s*\d+)?)"
    r"(?:\s*/\s*(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+)?|R\d+|G\d+))?"
    r"(?:\s*[（(][^）)]*[)）])?"
    r"\s*[：:]\s*")
# 2) 注释开头轮次标记（无冒号，后随空格+中文/ASCII 语义）：
PREFIX2 = re.compile(
    r"^(\s*//+\s*)(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+)?|R\d+|MICROFIX"
    r"(?:\s*#\s*\d+)?)(?:\s*/\s*(?:V\d+(?:\.\d+)?|R\d+|G\d+))?\s+")
# 3) 行中括号轮次标记：（V7 P7-1）(V14 (G7)) （R3）
PAREN = re.compile(
    r"[（(]\s*(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+)?|R\d+|MICROFIX"
    r"(?:\s*#\s*\d+)?)"
    r"(?:\s*/\s*(?:V\d+(?:\.\d+)?|R\d+|G\d+))?"
    r"(?:\s+[A-Za-z0-9/-]+)*\s*[)）]")
# 4) 行中裸轮次标记（词边界；不碰 NoiseWeightModelV1/astrocs-upm-v2）
TOKEN = re.compile(
    r"(?<![A-Za-z0-9_/-])(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+)?|R\d+|"
    r"MICROFIX(?:\s*#\s*\d+)?)"
    r"(?:\s*/\s*(?:V\d+(?:\.\d+)?|R\d+|G\d+))?"
    r"(?![A-Za-z0-9_/-])")
# 5) 中文历史短语
HIST = re.compile(r"(?:控制包|骨架版本|审计轮次|审计记录|号计划|"
                  r"第\s*\d+\s*号\s*计划|"
                  r"（\s*审计[^）]*）|审计\s*要求|审计\s*改名为|"
                  r"审计\s*示例|审计\s*§\s*\d+|源码审计\s*\+\s*|审计)")


def transform_line(raw: str) -> str:
    if "//" not in raw:
        return raw
    idx = raw.find("//")
    # 保护字符串字面量：// 若在引号内则跳过该行
    if raw.count('"', 0, idx) % 2 == 1 or raw.count("'", 0, idx) % 2 == 1:
        return raw
    code, comment = raw[:idx], raw[idx:]
    # 跳过含 // 的字符串（近似：URL/路径在注释外的代码行极少）
    m = PREFIX.match(comment)
    if m:
        comment = m.group(1) + comment[m.end():]
    else:
        m = PREFIX2.match(comment)
        if m:
            comment = m.group(1) + comment[m.end():]
    comment = PAREN.sub("", comment)
    comment = TOKEN.sub("", comment)
    comment = HIST.sub("", comment)
    comment = re.sub(r"\s{2,}", " ", comment)
    comment = comment.replace("移除 的", "移除的")
    comment = comment.replace("// ", "//").replace("//", "// ")
    comment = re.sub(r"//\s+[：:]\s*", "// ", comment)
    # 空注释清理
    tail = comment.strip(" /")
    if not tail:
        return code.rstrip() + "\n"
    return code + comment.rstrip() + "\n"


def main() -> int:
    r = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace",
                       timeout=120)
    files = [p for p in r.stdout.splitlines()
             if p.startswith("lib/") and p.endswith((".cpp", ".h", ".hpp",
                                                    ".c", ".cc", ".hh"))
             and not any(x in p.split("/")
                         for x in ("build", "build2", "_deps", "CMakeFiles",
                                   "archive", "third_party", "worktrees"))]
    changed = 0
    lines_changed = 0
    for p in files:
        path = os.path.join(ROOT, p)
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
        except OSError:
            continue
        out = []
        dirty = False
        for raw in lines:
            nxt = transform_line(raw)
            if nxt != raw:
                dirty = True
                lines_changed += 1
            out.append(nxt)
        if dirty:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.writelines(out)
            changed += 1
    print(f"comment strip: files_changed={changed} lines_changed={lines_changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
