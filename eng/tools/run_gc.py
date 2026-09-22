#!/usr/bin/env python3
"""run/ 轮次产物回收 —— 防止 run/ 无限膨胀（负责人 2026-09-21 指令）。

为什么需要：run/ 是 gitignore 的临时区，历史轮次（RELEASE-01/02/03、各任务 scratch）
会累积到上百 GB，挤压 L4 全流程所需工作盘。本脚本把"开新一轮先清旧轮次"变成固定动作。

用法：
  python3 eng/tools/run_gc.py                     # dry-run（默认）：只打印将删除的条目
  python3 eng/tools/run_gc.py --apply             # 实际删除
  python3 eng/tools/run_gc.py --keep RELEASE-04 --keep-glob 'FIX-*'
  python3 eng/tools/run_gc.py --min-free-gib 150  # 仅当可用空间低于阈值时，按最旧优先清理直到达标
  python3 eng/tools/run_gc.py --self-test         # 正/负例面（临时目录夹具，绝不触碰真 run/）

保留清单：eng/tools/run_keep.txt（tracked；每行一个 fnmatch glob，匹配 run/ 的直接子项名）。
推荐入口：eng/tools/round_start.sh <ROUND-ID>（回收 + 建目录 + 写轮次元数据，一步到位）。

路径单源（LINUXMAIN-PATH-01 A）：TOOLS_DIR = 本文件所在目录，REPO / RUN / KEEP_FILE 全部由它
派生，不再手抄 "tools/..." 字面量 —— 手抄副本一旦与真身漂移，保留清单会被**静默**当成不存在，
于是「保留清单命中」这条护栏整条失效（实测：RELEASE-04 被删）。

硬护栏（fail-closed，任一命中即拒绝该条目）：
  * 只处理 run/ 的**直接子项**，绝不递归跟随符号链接；
  * 拒绝符号链接；
  * 拒绝 git-tracked 路径（git ls-files 命中即拒）；
  * 拒绝 run/ 之外的任何路径（含 --protect 与 config 里声明的 output_dir）；
  * 保留清单（run_keep.txt + --keep/--keep-glob）命中的条目永不删除；
  * **保留清单缺失 / 解析为空 ⇒ ANCHOR_STALE + exit 2**，dry-run 与 --apply 一律拒绝：
    「清单读不到」不得被当成「清单为空」，否则空清单回收会删掉本应保留的轮次；
  * 仓库根推导自检：REPO 下必须有根 CMakeLists.txt（BLD-002 唯一根 CMake），否则 exit 2；
  * 无 --apply 时只打印，不动任何文件。

退出码：0 = 正常；1 = 一般错误；2 = ANCHOR_STALE（锚失效 / 输入缺失，fail-closed）。
"""
from __future__ import annotations

import argparse
import fnmatch
import json
import shutil
import subprocess
import sys
from pathlib import Path

# 路径单源：TOOLS_DIR 是唯一手写锚，其余全部派生（禁第二份 "tools/..." 字面量副本）。
TOOLS_DIR = Path(__file__).resolve().parent          # eng/tools
REPO = TOOLS_DIR.parent.parent                        # eng/tools -> eng -> 仓库根
RUN = REPO / "run"
KEEP_FILE = TOOLS_DIR / "run_keep.txt"                # 与 run_gc.py 同目录，永不漂移
ROOT_SENTINEL = REPO / "CMakeLists.txt"               # 仓库根判据（BLD-002 唯一根 CMake）


class KeepListUnavailable(Exception):
    """保留清单缺失/为空 —— fail-closed，拒绝回收（ANCHOR_STALE，exit 2）。"""


def assert_repo_layout() -> None:
    """仓库根推导自检：根 CMakeLists.txt 不在 ⇒ 路径推导错，fail-closed 退出 2。"""
    if not ROOT_SENTINEL.is_file():
        raise KeepListUnavailable(
            "ANCHOR_STALE: ROOT_SENTINEL " + str(ROOT_SENTINEL)
            + " 不存在 —— 仓库根推导错误（REPO 推导为 " + str(REPO) + "），拒绝继续")


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
    """读保留清单；缺失 / 解析为空 ⇒ KeepListUnavailable（fail-closed，不返回空清单）。

    LINUXMAIN-PATH-01 A 判据：原实现 `if keep_file.is_file()` 静默跳过读不到的清单，
    等价于「保留清单 = 空」⇒ --apply 会回收本应保留的轮次（实测 RELEASE-04 被删）。
    保留清单是本脚本唯一的「不可删」来源，读不到时**没有任何安全默认值** ⇒ 必须拒绝执行。
    """
    if not keep_file.is_file():
        raise KeepListUnavailable(
            "ANCHOR_STALE: KEEP_FILE " + str(keep_file)
            + " 不存在 —— 保留清单不可读；按空清单回收会删除本应保留的轮次，"
              "fail-closed 拒绝执行")
    pats = []
    for line in keep_file.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            pats.append(line)
    if not pats:
        raise KeepListUnavailable(
            "ANCHOR_STALE: KEEP_FILE " + str(keep_file)
            + " 为空（无任何保留 glob）—— 空清单回收会删除 run/ 下全部轮次，"
              "fail-closed 拒绝执行")
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


def selftest() -> int:
    """机器可执行正/负例面（ENGINEERING_SPEC §10「能红能绿」）。

    全程在临时目录夹具内执行：复制本脚本与真保留清单进夹具，夹具自带 run/ 与根
    CMakeLists.txt ⇒ 绝不触碰真 run/ 与真仓库。
    负例：① 保留清单缺失 ② 保留清单存在但为空 ③ 仓库根推导错误 ④ 清单缺失时 dry-run。
    正例：清单可读非空 ⇒ 清单命中项 KEEP 不删、未命中项 WOULD-DELETE/DELETE。
    """
    import tempfile

    fails = []
    real_tools = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        tools = root / "eng" / "tools"
        tools.mkdir(parents=True)
        (root / "CMakeLists.txt").write_text("# stub root cmake\n", encoding="utf-8")
        run = root / "run"
        for name in ("RELEASE-04", "DOC-402", "scratch-01"):
            (run / name / "logs").mkdir(parents=True)
            (run / name / "logs" / "a.log").write_text("x", encoding="utf-8")
        gc = tools / "run_gc.py"
        keep = tools / "run_keep.txt"
        shutil.copy2(Path(__file__).resolve(), gc)
        shutil.copy2(real_tools / "run_keep.txt", keep)

        def call(argv):
            return subprocess.run([sys.executable, str(gc)] + argv, cwd=str(root),
                                  capture_output=True, text=True, timeout=180)

        def blob(r):
            return r.stdout + r.stderr

        # ── 负例 ①：保留清单缺失 ⇒ --apply 必须 exit 2 且点名 KEEP_FILE ──
        keep.unlink()
        r = call(["--apply"])
        if r.returncode != 2:
            fails.append("负例1 保留清单缺失未被拒绝: rc=%d" % r.returncode)
        if "ANCHOR_STALE" not in blob(r) or "KEEP_FILE" not in blob(r):
            fails.append("负例1 未点名 ANCHOR_STALE/KEEP_FILE: %s" % blob(r)[-200:])
        if not (run / "RELEASE-04").is_dir():
            fails.append("负例1 清单缺失时仍删除了条目（fail-closed 未生效）")

        # ── 负例 ②：保留清单缺失 ⇒ dry-run 同样拒绝（不得静默列出全部可删）──
        r = call([])
        if r.returncode != 2 or "ANCHOR_STALE" not in blob(r):
            fails.append("负例2 清单缺失时 dry-run 未拒绝: rc=%d" % r.returncode)

        # ── 负例 ③：保留清单存在但为空（仅注释）⇒ 必须拒绝 ──
        keep.write_text("# 只有注释\n\n", encoding="utf-8")
        r = call(["--apply"])
        if r.returncode != 2 or "ANCHOR_STALE" not in blob(r):
            fails.append("负例3 空清单未被拒绝: rc=%d" % r.returncode)
        if not (run / "RELEASE-04").is_dir():
            fails.append("负例3 空清单时仍删除了条目")

        # ── 负例 ④：仓库根推导错误（根 CMakeLists.txt 不在）⇒ 拒绝 ──
        keep.write_text("RELEASE-04\n", encoding="utf-8")
        (root / "CMakeLists.txt").unlink()
        r = call(["--apply"])
        if r.returncode != 2 or "ROOT_SENTINEL" not in blob(r):
            fails.append("负例4 根推导错误未被拒绝: rc=%d" % r.returncode)
        (root / "CMakeLists.txt").write_text("# stub root cmake\n", encoding="utf-8")

        # ── 正例：清单可读非空 ⇒ dry-run 保留命中项、--apply 只删未命中项 ──
        shutil.copy2(real_tools / "run_keep.txt", keep)   # 复原真清单（负例③④改过）
        r = call([])
        if r.returncode != 0:
            fails.append("正例 dry-run rc=%d" % r.returncode)
        if "KEEP   RELEASE-04" not in r.stdout:
            fails.append("正例 dry-run 未保留清单命中项 RELEASE-04: %s" % r.stdout[-300:])
        if "WOULD-DELETE scratch-01" not in r.stdout:
            fails.append("正例 dry-run 未列出未命中项 scratch-01: %s" % r.stdout[-300:])
        r = call(["--apply"])
        if r.returncode != 0:
            fails.append("正例 --apply rc=%d" % r.returncode)
        if not (run / "RELEASE-04").is_dir() or not (run / "DOC-402").is_dir():
            fails.append("正例 保留清单命中项被误删")
        if (run / "scratch-01").exists():
            fails.append("正例 未命中项未被回收")

    if fails:
        print("RUN_GC_SELFTEST_FAIL:")
        for x in fails:
            print("  " + x)
        return 1
    print("RUN_GC_SELFTEST_PASS: 清单缺失/为空/根推导错误各自 fail-closed(exit 2)；"
          "清单命中项不删、未命中项回收")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="run/ 轮次产物回收")
    ap.add_argument("--apply", action="store_true", help="实际删除（默认 dry-run）")
    ap.add_argument("--keep", action="append", default=[], help="保留的 run/ 子项名（可重复）")
    ap.add_argument("--keep-glob", action="append", default=[], help="保留的 fnmatch glob（可重复）")
    ap.add_argument("--keep-file", default=str(KEEP_FILE), help="保留清单路径")
    ap.add_argument("--protect", action="append", default=[], help="额外保护的路径（可重复）")
    ap.add_argument("--min-free-gib", type=float, default=None,
                    help="仅当可用空间低于该值时清理（按最旧优先，直到达标）")
    ap.add_argument("--self-test", action="store_true",
                    help="机器可执行正/负例面（临时目录夹具；不触碰真 run/）")
    args = ap.parse_args()

    if args.self_test:
        return selftest()

    try:
        assert_repo_layout()
        pats = load_keep_patterns(Path(args.keep_file), args.keep, args.keep_glob)
    except KeepListUnavailable as exc:
        # fail-closed：dry-run 与 --apply 一律拒绝（ENGINEERING_SPEC §10 输入缺失即判红）
        print(str(exc), file=sys.stderr)
        return 2

    if not RUN.is_dir():
        print(f"run/ 不存在：{RUN}（无轮次产物可回收）")
        return 0

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
