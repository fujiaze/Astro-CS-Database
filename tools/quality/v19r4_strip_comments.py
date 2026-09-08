#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""v19r4_strip_comments.py — V19R4 comment hygiene 残留清理。

处理（仅注释文本；保留科学语义，去轮次/修补/哈希历史标记）：
1. V19R4（xxx）：→ 去轮次前缀，保留括号语义（或改写为语义短语）
2. R0x-xxx（R07-M09/R04-B17 等）：→ 去 ID
3. GAP-0xx：→ 语义化
4. 控制包 SHA（34A532A2...）：→ 删除
5. V18R2/V18R3/V19R2 等轮次 token（生产文件）

测试文件保留稳定测试 ID（F-V19R2-*/TEST-*/SNR-*/UPMW-*）与正式格式
引用（02_FROZEN/格式布局），但去掉 R0x-Bxx 轮次 ID。
"""

from __future__ import annotations

import argparse
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


def _atomic_write_lines(path: str, lines: list[str]) -> None:
    # 原子写：tmp 与目标同目录（同一文件系统，os.replace 才是原子 rename）。
    # 写全 + flush + fsync 后一次性 rename；任何时刻被 SIGKILL，目标要么是
    # 原文要么是新全文，绝不截断。残留的 *.strip-tmp 由下次执行清理。
    tmp = f"{path}.strip-tmp"
    data = "".join(lines)
    try:
        with open(tmp, "w", encoding="utf-8", newline="") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise

ROOT = _deduce_root()

# 1) 行首/行中括号轮次前缀：V19R4（xxx）→ （xxx）
ROUND_PREFIX = re.compile(
    r"(?<![A-Za-z0-9_-])(?:V\d+(?:\.\d+)?R\d+|V18R[0-9]|V19R[0-9])"
    r"(?:\s*[（(]([^）)]*)[)）])?")
# 2) R0x-xxx / GAP-0xx / P1x-xxx 轮次-修补 ID
TASKID = re.compile(
    r"\b(?:R\d{2}-[A-Z0-9/]+|GAP-\d{3}|P\d{2}-\d{3})\b")
# 3) 控制包 SHA 前缀（40 hex）
SHA = re.compile(r"\b[0-9a-fA-F]{40}(?:\.\.\.)?\b")
# 4) 中文历史短语（本轮引入的轮次词）
HIST = re.compile(r"V19R4\s*[：:]|（\s*V19R4\s*[）)]|本轮修复|本轮")


def is_test_file(p: str) -> bool:
    return "tests/" in p or "test/" in p or p.endswith("_test.cpp")


def transform_line(p: str, raw: str) -> str:
    if "//" not in raw:
        return raw
    idx = raw.find("//")
    if raw.count('"', 0, idx) % 2 == 1 or raw.count("'", 0, idx) % 2 == 1:
        return raw
    code, comment = raw[:idx], raw[idx:]
    # 保留括号语义内容：V19R4（K_CORR_DOMAIN 选项 B）→ K_CORR_DOMAIN 选项 B
    comment = ROUND_PREFIX.sub(
        lambda m: ("（" + m.group(1) + "）") if m.group(1) else "",
        comment)
    comment = TASKID.sub("", comment)
    comment = SHA.sub("", comment)
    comment = HIST.sub("", comment)
    # 清理残留
    comment = re.sub(r"//+[\s]*[：:]\s*//+", "//", comment)
    comment = re.sub(r"[（(]\s*[)）]", "", comment)
    comment = re.sub(r"\s{2,}", " ", comment)
    comment = re.sub(r"\s+([：:，。；）)])", r"\1", comment)
    comment = comment.rstrip() + "\n"
    return code + comment


def main() -> int:
    ap = argparse.ArgumentParser(description="v19r4_strip_comments.py — V19R4 comment hygiene 残留清理")
    ap.add_argument("--root", default=None,
                    help="目标仓库根目录（默认沿用 _deduce_root() 推断；"
                         "供对临时副本安全测试用）")
    args = ap.parse_args()
    root = args.root if args.root else ROOT
    files = [p for p in subprocess.run(
        ["git", "ls-files"], cwd=root, capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=120).stdout.splitlines()
        if p.startswith("lib/") and p.endswith((".cpp", ".h", ".hpp", ".cu"))
        and not any(x in p.split("/")
                    for x in ("build", "build2", "_deps", "CMakeFiles",
                              "archive", "third_party", "worktrees"))]
    changed = []
    for p in files:
        path = os.path.join(root, p)
        try:
            with open(path, encoding="utf-8") as f:  # strict：非法 UTF-8 直接抛错
                lines = f.readlines()
        except (OSError, UnicodeDecodeError) as e:
            # 跳过该文件：绝不写回，也绝不用 errors="replace" 重读
            print(f"[skip] {p}: {type(e).__name__}: {e}", file=sys.stderr)
            continue
        new = [transform_line(p, ln) for ln in lines]
        if new != lines:
            # 仅清理历史残留 tmp（本次原子 os.replace 会腾出该名字），
            # 覆盖上一轮异常退出遗留的 *.strip-tmp
            if os.path.exists(path + ".strip-tmp"):
                os.unlink(path + ".strip-tmp")
            _atomic_write_lines(path, new)
            changed.append(p)
    print(f"changed files: {len(changed)}")
    for p in changed:
        print(" ", p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
