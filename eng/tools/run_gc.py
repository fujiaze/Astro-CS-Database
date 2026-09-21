#!/usr/bin/env python3
"""run/ 轮次产物回收 —— 防止 run/ 无限膨胀（负责人 2026-09-21 指令）。

为什么需要：run/ 是 gitignore 的临时区，历史轮次（RELEASE-01/02/03、各任务 scratch）
会累积到上百 GB，挤压 L4 全流程所需工作盘。本脚本把"开新一轮先清旧轮次"变成固定动作。

用法：
  python3 tools/run_gc.py                       # dry-run（默认）：只打印将删除的条目
  python3 tools/run_gc.py --apply               # 实际删除
  python3 tools/run_gc.py --keep RELEASE-04 --keep-glob 'FIX-*'
  python3 tools/run_gc.py --min-free-gib 150    # 仅当可用空间低于阈值时，按最旧优先清理直到达标

保留清单：tools/run_keep.txt（tracked；每行一个 fnmatch glob，匹配 run/ 的直接子项名）。
推荐入口：tools/round_start.sh <ROUND-ID>（回收 + 建目录 + 写轮次元数据，一步到位）。

硬护栏（fail-closed，任一命中即拒绝该条目）：
  * 只处理 run/ 的**直接子项**，绝不递归跟随符号链接；
  * 拒绝符号链接；
  * 拒绝 git-tracked 路径（git ls-files 命中即拒）；
  * 拒绝 run/ 之外的任何路径（含 --protect 与 config 里声明的 output_dir）；
  * 保留清单（run_keep.txt + --keep/--keep-glob）命中的条目永不删除；
  * 无 --apply 时只打印，不动任何文件。
"""
from __future__ import annotations

import argparse
import fnmatch
import json
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
RUN = REPO / "run"
KEEP_FILE = REPO / "tools" / "run_keep.txt"


def _human(n: int) -> str:
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if n < 1024 or unit == "TiB":
            return f"{n:.1f} {unit}" if unit != "B" else f"{n} B"
        n /= 1024.0
    return f"{n:.1f} TiB"


def _dir_size(p: Path) -> int:
    total = 0
    for root, dirs, files in p.walk() if hasattr(p, "walk") else _walk(p):
        for f in files:
            try:
                total += (Path(root) / f).stat().st_size
            except OSError:
                pass
    return total


def _walk(p: Path):
    import os
    for root, dirs, files in os.walk(p, followlinks=False):
        yield root, dirs, files


def load_keep_patterns(keep_file: Path, extra_keep, extra_glob):
    pats = []
    if keep_file.is_file():
        for line in keep_file.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                pats.append(line)
    pats.extend(extra_keep or [])
    pats.extend(extra_glob or [])
    return pats


def kept(name: str, pats) -> bool:
    return any(fnmatch.fnmatch(name, p) for p in pats)


def git_tracked(rel: str) -> bool:
    try:
        out = subprocess.run(
            ["git", "-c", "core.quotepath=false", "ls-files", "--", rel],
            cwd=REPO, capture_output=True, text=True, timeout=60)
        return bool(out.stdout.strip())
    except Exception:
        # git 不可用 ⇒ 保守拒绝删除（fail-closed）
        return True


def main() -> int:
    ap = argparse.ArgumentParser(description="run/ 轮次产物回收")
    ap.add_argument("--apply", action="store_true", help="实际删除（默认 dry-run）")
    ap.add_argument("--keep", action="append", default=[], help="保留的 run/ 子项名（可重复）")
    ap.add_argument("--keep-glob", action="append", default=[], help="保留的 fnmatch glob（可重复）")
    ap.add_argument("--keep-file", default=str(KEEP_FILE), help="保留清单路径")
    ap.add_argument("--protect", action="append", default=[], help="额外保护的路径（可重复）")
    ap.add_argument("--min-free-gib", type=float, default=None,
                    help="仅当可用空间低于该值时清理（按最旧优先，直到达标）")
    args = ap.parse_args()

    if not RUN.is_dir():
        print(f"run/ 不存在：{RUN}")
        return 0

    pats = load_keep_patterns(Path(args.keep_file), args.keep, args.keep_glob)
    protect = {Path(p).resolve() for p in args.protect}

    free_before = shutil.disk_usage(RUN).free
    need = 0
    if args.min_free_gib is not None:
        target = int(args.min_free_gib * 1024**3)
        if free_before >= target:
            print(f"可用空间 {_human(free_before)} >= 阈值 {args.min_free_gib} GiB ⇒ 无需清理")
            return 0
        need = target - free_before

    entries = []
    for child in sorted(RUN.iterdir(), key=lambda p: p.stat().st_mtime if p.exists() else 0):
        name = child.name
        if name == ".gitkeep":
            continue
        if kept(name, pats):
            print(f"KEEP   {name}（保留清单命中）")
            continue
        if child.is_symlink():
            print(f"SKIP   {name}（符号链接，拒绝）")
            continue
        rel = str(child.relative_to(REPO))
        if git_tracked(rel):
            print(f"SKIP   {name}（git-tracked，拒绝）")
            continue
        if child.resolve() in protect or not str(child.resolve()).startswith(str(RUN.resolve())):
            print(f"SKIP   {name}（受保护或在 run/ 之外）")
            continue
        entries.append((child, _dir_size(child) if child.is_dir() else child.stat().st_size))

    if not entries:
        print("无可回收条目。")
        return 0

    total = sum(sz for _, sz in entries)
    print(f"\n可回收 {len(entries)} 项，共 {_human(total)}（run/ 可用空间 {_human(free_before)}）")
    freed = 0
    for child, sz in entries:
        if need and freed >= need:
            print(f"STOP   {child.name}（已达 --min-free-gib 目标）")
            break
        action = "DELETE" if args.apply else "WOULD-DELETE"
        print(f"{action} {child.name}  {_human(sz)}")
        if args.apply:
            if child.is_dir():
                shutil.rmtree(child, ignore_errors=False)
            else:
                child.unlink()
        freed += sz

    free_after = shutil.disk_usage(RUN).free
    verb = "已回收" if args.apply else "可回收"
    print(f"\n{verb} {_human(freed)}；run/ 可用空间 {_human(free_before)} → {_human(free_after)}")
    if not args.apply:
        print("（dry-run；加 --apply 才实际删除）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
