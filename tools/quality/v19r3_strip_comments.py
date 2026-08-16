#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""v19r3_strip_comments.py — V19R3 comment hygiene 机械剥离（DOCS_AND_COMMENTS §13）。

处理规则（保留科学语义，只去历史/过程标记）：
1. 注释开头轮次前缀：Vxx / Rxx / MICROFIX / V18R2 (CODE-00x) / V19R2
   / F-V19R2-XXX-00N（生产注释中保留语义、去 ID；测试文件保留 ID 作
   稳定测试标识）。
2. 行中括号标记：（聚焦版 v2/v3）（08 控制包/审计）等。
3. 任务号 P0x-00x 注释前缀/后缀（生产注释去 ID，保留语义）。
4. 中文历史短语：控制包/审计/骨架版本/聚焦版/Full Freeze/本轮修复。
5. 陈旧语义短语：骨架实现 → 基础实现（仅当该行确实过时时人工修正，
   本工具仅处理可机械判断的模式）。

应用后必须：全量重编译 + 重跑测试 + v19r3_audit.py 复扫。
"""

from __future__ import annotations

import os
import re
import subprocess
import sys

ROOT = r"F:\Astro dev\Astro CS Normalization Database"

# 1) 注释开头轮次前缀 + 冒号/空格
PREFIX = re.compile(
    r"^(\s*//+\s*)"
    r"(?:F-)?(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+|[A-Za-z]?\d+)?|R\d+|MICROFIX"
    r"(?:\s*#\s*\d+)?)(?:\s*\(CODE-\d+\))?"
    r"(?:\s*[（(][^）)]*[)）])?"
    r"\s*[：:]\s*")
# 2) 行中括号轮次/聚焦版/控制包标记
PAREN = re.compile(
    r"[（(]\s*(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+|[A-Za-z]?\d+)?|R\d+|MICROFIX|"
    r"F-V19R2-[A-Z0-9-]+|聚焦版(?:\s*v\d+)?|控制包|审计|"
    r"\d+\s*[号份]\s*(?:控制包|计划))"
    r"(?:\s*/\s*(?:V\d+(?:\.\d+)?|R\d+|G\d+))?"
    r"(?:\s+[A-Za-z0-9/-]+)*\s*[)）]")
# 3) 裸轮次 token（不碰 NoiseWeightModelV1/astrocs-upm-v2/FITS 等）
TOKEN = re.compile(
    r"(?<![A-Za-z0-9_/-])(?:F-)?(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+|[A-Za-z]?\d+)?|R\d+|"
    r"MICROFIX(?:\s*#\s*\d+)?)(?:\s*\(CODE-\d+\))?"
    r"(?![A-Za-z0-9_/-])")
# 4) 任务号（P0x-00x 或 P0x-00x:）
TASKID = re.compile(
    r"(^|[\s(（])P\d{2}-\d{3}"
    r"(?:\s*[：:])?\s*")
# 残留轮次-审计 ID（V19R2-CUDA-001 等；生产注释去 ID 保留语义）
LEFTOVER_ID = re.compile(r"V\d+(?:\.\d+)?R\d+-[A-Z0-9-]+\d{3}")
# 5) 中文历史短语（机械安全子集）
HIST = re.compile(r"(?:控制包|审计轮次|审计记录|审计\s*[§:：]|号计划|"
                  r"聚焦版(?:\s*v\d+)?|Full Freeze|本轮修复|本轮|"
                  r"后续\s*Task|后续任务|Task\s*实现)")


def is_test_file(p: str) -> bool:
    return "tests/" in p or p.endswith("_test.cpp") or "test/" in p


def transform_line(p: str, raw: str) -> str:
    if "//" not in raw:
        return raw
    idx = raw.find("//")
    if raw.count('"', 0, idx) % 2 == 1 or raw.count("'", 0, idx) % 2 == 1:
        return raw
    code, comment = raw[:idx], raw[idx:]
    # 测试文件保留 F-V19R2-* ID（稳定测试标识）；生产文件去 ID 保留语义
    if not is_test_file(p):
        comment = comment.replace("F-V19R2-", "V19R2-")
        comment = LEFTOVER_ID.sub("", comment)
    comment = PREFIX.sub(r"\1", comment)
    comment = PAREN.sub("", comment)
    comment = TOKEN.sub("", comment)
    comment = TASKID.sub(r"\1", comment)
    comment = HIST.sub("", comment)
    # 清理残留（仅注释文本内，绝不触碰代码部分）：
    # 1) 注释标记后孤儿冒号：// ： → //
    comment = re.sub(r"//+[\s]*[：:]", "//", comment)
    # 2) 横线后孤儿冒号：---- ： → ----
    comment = re.sub(r"(-{2,})\s*[：:]", r"\1", comment)
    # 3) 控制包节引用（08 §5 / 05 号规范 §2）——历史过程标记
    comment = re.sub(
        r"[（(]\s*(?:\d{2}\s*[§号]\s*[\d/]+(?:\s*/\s*\d{2}\s*[号]?\s*"
        r"规范\s*§\s*[\d/]+)?|(?:\d{2}\s*号\s*规范(?:\s*§\s*[\d/]+)?)|"
        r"(?:ACR\s*架构\s*冻结(?:\s*§\s*[\d/]+)?))[)）]",
        "", comment)
    # 4) 双空格/行尾空格
    comment = re.sub(r"\s{2,}", " ", comment)
    comment = comment.rstrip() + "\n"
    return code + comment


def main() -> int:
    files = [p for p in subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=120).stdout.splitlines()
        if p.startswith("lib/") and p.endswith((".cpp", ".h", ".hpp", ".cu"))
        and not any(x in p.split("/")
                    for x in ("build", "build2", "_deps", "CMakeFiles",
                              "archive", "third_party", "worktrees"))]
    changed = []
    for p in files:
        path = os.path.join(ROOT, p)
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
        except OSError:
            continue
        new = [transform_line(p, ln) for ln in lines]
        if new != lines:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.writelines(new)
            changed.append(p)
            print(f"[stripped] {p}")
    print(f"changed files: {len(changed)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
