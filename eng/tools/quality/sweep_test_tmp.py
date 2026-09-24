#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""sweep_test_tmp.py —— 清理 ACSD 测试夹具的临时目录残留（fail-soft）。

为什么需要它（实测根因，2026-09-20）：
  * 78 个测试用 tempfile.mkdtemp() 建夹具，其中 15 个**完全没有清理**；
  * 即使有 tearDownClass/atexit 的，在**崩溃或超时**时也不会执行；
  * 单次全量门禁可残留 ~7.5 GB（/dev/shm 871 个目录；p2007 单个 2.3 GB）；
  * 后果不只是占空间：**编译写临时文件失败 ⇒ 二进制不更新 ⇒ 陈旧二进制给出假红**
    （实证：RT001 崩 nlohmann json type_error.306，清理后重建 ⇒ PASS）。

同时：**不要把 TMPDIR 指向 tmpfs**（/dev/shm、/tmp 各只有 7.9 GB，且 /dev/shm 吃 RAM）。
本机磁盘位：/ 218 GB 空闲、/workspace 275 GB 空闲。推荐 TMPDIR=/var/tmp/astrocs。

用法：
  python3 eng/tools/quality/sweep_test_tmp.py                  # 干跑，只报告
  python3 eng/tools/quality/sweep_test_tmp.py --apply          # 真删
  python3 eng/tools/quality/sweep_test_tmp.py --apply --min-age 2   # 只删 2 分钟前的（默认 5）
  python3 eng/tools/quality/sweep_test_tmp.py --roots /dev/shm /tmp /var/tmp/astrocs
"""
from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import sys
import time

# ACSD 测试夹具的前缀（与 eng/tests/** 中 mkdtemp(prefix=...) 实测一致）
PREFIXES = (
    "p1001_", "p1004_", "p2001_", "p2002_", "p2006_", "p2007_", "p3004_", "p3005_",
    "p3006_", "p3rs_", "p1hips_", "p1star_", "p1snr_", "cpu001_", "aio_abi_",
    "astrocs_", "syn0", "par0", "mon001_", "hstcal", "dz", "fix_p2a", "fix-p2b",
)
DEFAULT_ROOTS = ("/dev/shm", "/tmp", "/var/tmp/astrocs")


def sweep(roots, min_age_s, apply_changes, verbose=True):
    now = time.time()
    removed, kept, freed = [], [], 0
    for root in roots:
        rp = pathlib.Path(root)
        if not rp.is_dir():
            continue
        for child in sorted(rp.iterdir()):
            if not child.name.startswith(PREFIXES):
                continue
            try:
                age = now - child.stat().st_mtime
            except OSError:
                continue
            if age < min_age_s:
                kept.append(str(child))
                continue
            size = 0
            try:
                if child.is_dir():
                    for f in child.rglob("*"):
                        try:
                            if f.is_file():
                                size += f.stat().st_size
                        except OSError:
                            pass
                else:
                    size = child.stat().st_size
            except OSError:
                pass
            if apply_changes:
                try:
                    shutil.rmtree(child) if child.is_dir() else child.unlink()
                except OSError as exc:
                    if verbose:
                        print("  SKIP %s (%s)" % (child, exc), file=sys.stderr)
                    continue
            removed.append((str(child), size))
            freed += size
    if verbose:
        for path, size in removed:
            print("  %s %-60s %.1f MB" % ("REMOVED" if apply_changes else "WOULD REMOVE",
                                          path, size / 1048576.0))
        for path in kept:
            print("  KEPT (in use) %s" % path)
        print("sweep: %s %d entries, %.2f GB%s"
              % ("removed" if apply_changes else "would remove", len(removed),
                 freed / 1073741824.0, "" if apply_changes else " (dry-run)"))
    return removed, kept, freed


def _self_test() -> int:
    import tempfile
    fails = []
    with tempfile.TemporaryDirectory() as td:
        root = pathlib.Path(td)
        (root / "p2001_aaa").mkdir()
        (root / "p2001_aaa" / "x.bin").write_bytes(b"0" * 4096)
        (root / "not_a_fixture").mkdir()
        (root / "p2002_old").mkdir()
        os.utime(root / "p2002_old", (0, 0))
        removed, kept, freed = sweep([td], 300, False, verbose=False)
        if len(removed) != 1 or "p2002_old" not in removed[0][0]:
            fails.append("dry-run 应只选中超龄的 p2002_old，实得 %r" % (removed,))
        if not (root / "p2002_old").exists():
            fails.append("dry-run 不得真删")
        if (root / "not_a_fixture").exists() is False:
            fails.append("非夹具目录被误删")
        if not any("p2001_aaa" in k for k in kept):
            fails.append("p2001_aaa 是新建的（age < min_age）⇒ 必须在 kept 里，实得 %r" % (kept,))
        removed2, _, _ = sweep([td], 300, True, verbose=False)
        if (root / "p2002_old").exists():
            fails.append("--apply 后 p2002_old 应已删除")
        if not (root / "not_a_fixture").exists():
            fails.append("--apply 误删非夹具目录")
        if not (root / "p2001_aaa").exists():
            fails.append("--apply 不得删未超龄目录")
    # 负例：空 roots 不得崩
    r, k, f = sweep(["/nonexistent-xyz"], 0, False, verbose=False)
    if r or k or f:
        fails.append("不存在的 root 应返回空")
    if fails:
        print("SELFTEST FAIL:")
        for x in fails:
            print("  - " + x)
        return 1
    print("SELFTEST PASS (sweep_test_tmp)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--roots", nargs="*", default=list(DEFAULT_ROOTS))
    ap.add_argument("--min-age", type=float, default=300.0,
                    help="只清理 mtime 早于 N 秒的条目（默认 300，避免误删在跑的）")
    ap.add_argument("--apply", action="store_true", help="真删；缺省为干跑")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return _self_test()
    sweep(args.roots, args.min_age, args.apply, verbose=not args.quiet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())