#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_release_layout.py — REL-001 发布布局校验（**已退役，非门禁**）。

退役判定依据
  1) 判据面已被在册门承接：发布布局 / 白名单 / checksums / SBOM 的机器判据在
     eng/ci/checks.json 的 CHK-PKG-CONSISTENCY（PKG-CONSISTENCY / PKG-SBOM 及其
     -NEG 负例面）中；本脚本在 eng/ci/checks.json 与 docs/ci/01_CHECKS.md §2 中零引用。
  2) 判据对象不存在：唯一输入 dist/astrocs-alpha 是发布候选产物目录，现不存在
     （发布决定只属负责人，AGENTS.md §6）。
  3) 行为缺陷（本次一并订正）：原实现在 dist 缺失分支里先 errors.append 再 return 1，
     位置在**任何打印之前** ⇒ 无参调用 rc=1 且零输出，违反 docs/ci/01_CHECKS.md §1:14
     「失效时显式失败并点名，不得静默降级」。

退役契约（docs/ci/01_CHECKS.md §2.1「检查器退役与预留」）
  * 无参调用：打印退役标识并 exit 2；
  * 原实现保留为 legacy_main()，经 --legacy-check 复跑（判据未改，只补具名输出）；
  * 本检查器不写任何产物文件 ⇒ 不存在「产物落仓库根」的问题。

退出码：0 = --legacy-check 通过；1 = --legacy-check 判据违规；
        2 = 退役（无参调用）/ ANCHOR_STALE（锚失效，fail-closed）；3 = 用法错误。
"""
from __future__ import annotations

import argparse
import hashlib
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ── 判据锚（docs/ci/01_CHECKS.md §1:14 锚存活） ────────────────────────────
DIST_REL = os.path.join("dist", "astrocs-alpha")
REQUIRED_FILES = ("VERSION", "LICENSE", "README.txt", "checksums.sha256")
BANNED_DIRS = ("build", "testdata", ".git", "history")

RETIREMENT_MARKER = (
    "RETIRED: check_release_layout.py（REL-001）未注册进 eng/ci/checks.json；"
    "发布布局/白名单/checksums/SBOM 的在册门是 CHK-PKG-CONSISTENCY（PKG-CONSISTENCY/PKG-SBOM）。"
)


def legacy_main(dist: str) -> int:
    """退役前的原判定实现（判据未改；只把原「无输出 return 1」补成具名输出）。"""
    errors: list[str] = []
    if not os.path.isdir(dist):
        print("ANCHOR_STALE: DIST_REL %s（发布候选产物目录不存在）"
              % os.path.relpath(dist, REPO).replace(os.sep, "/"), file=sys.stderr)
        return 2

    exes = [p for p in os.listdir(dist)
            if os.path.isfile(os.path.join(dist, p))
            and os.stat(os.path.join(dist, p)).st_mode & 0o111]
    if len(exes) != 1 or exes[0] != "acsd":
        errors.append("非单一入口: %s" % sorted(exes))
    for req in REQUIRED_FILES:
        if not os.path.isfile(os.path.join(dist, req)):
            errors.append("missing %s" % req)
    for dirpath, dirnames, _files in os.walk(dist):
        for name in list(dirnames):
            if name in BANNED_DIRS:
                errors.append("不应含 %s（%s）"
                              % (name, os.path.relpath(os.path.join(dirpath, name), dist)))
    cs_path = os.path.join(dist, "checksums.sha256")
    if os.path.isfile(cs_path):
        with open(cs_path, encoding="utf-8", errors="replace") as fh:
            for line in fh.read().strip().splitlines():
                parts = line.split()
                if len(parts) != 2:
                    errors.append("checksums.sha256 行格式非法: %r" % line.strip()[:80])
                    continue
                h, name = parts
                target = os.path.join(dist, name)
                if not os.path.isfile(target):
                    errors.append("checksum 指向不存在文件 %s" % name)
                    continue
                with open(target, "rb") as bf:
                    real = hashlib.sha256(bf.read()).hexdigest()
                if h != real:
                    errors.append("checksum mismatch %s" % name)
    if errors:
        print("REL-001_LAYOUT_VIOLATION (%d):" % len(errors))
        for e in errors:
            print("  " + e)
        return 1
    print("REL-001_PASS: 单一入口 acsd, 必含文件齐, checksum 一致, 无 build/testdata/history")
    return 0


# ────────────────────────────────────────────────────────────── self-test
def _run_cli(args: list[str]) -> "subprocess.CompletedProcess":
    import subprocess
    return subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                          capture_output=True, text=True, timeout=300)


def _mk_dist(root: str, exes=("acsd",), files=REQUIRED_FILES, bad_checksum=False,
             banned=()) -> str:
    dist = os.path.join(root, DIST_REL)
    os.makedirs(dist, exist_ok=True)
    for e in exes:
        p = os.path.join(dist, e)
        with open(p, "wb") as fh:
            fh.write(b"fake-exe\n")
        os.chmod(p, 0o755)
    for f in files:
        if f == "checksums.sha256":
            continue
        with open(os.path.join(dist, f), "w", encoding="utf-8") as fh:
            fh.write("x\n")
    lines = []
    for f in files:
        if f == "checksums.sha256":
            continue
        with open(os.path.join(dist, f), "rb") as bf:
            digest = hashlib.sha256(bf.read()).hexdigest()
        if bad_checksum and f == "VERSION":
            digest = "0" * 64
        lines.append("%s  %s" % (digest, f))
    with open(os.path.join(dist, "checksums.sha256"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    for b in banned:
        os.makedirs(os.path.join(dist, b), exist_ok=True)
    return dist


def _self_test() -> int:
    """可执行正/负例面：退役契约 + 原判据的能红能绿（§1:10–11）。"""
    import shutil
    import tempfile

    problems: list[str] = []
    cases = 0

    def check(name: str, rc: int, want_rc: int, blob: str, token: str) -> None:
        nonlocal cases
        cases += 1
        ok = rc == want_rc and token in blob
        print("  SELFTEST_%s %-34s rc=%d want_rc=%d token=%r"
              % ("PASS" if ok else "FAIL", name, rc, want_rc, token))
        if not ok:
            problems.append("%s: rc=%d(want %d) token=%r missing" % (name, rc, want_rc, token))

    r = _run_cli([])
    check("P0_retired_noarg", r.returncode, 2, r.stdout + r.stderr, "RETIRED")

    with tempfile.TemporaryDirectory(prefix="rel_layout_st_") as td:
        dist = _mk_dist(td)
        r = _run_cli(["--legacy-check", "--dist", dist])
        check("P1_legacy_green", r.returncode, 0, r.stdout + r.stderr, "REL-001_PASS")

        shutil.rmtree(dist)
        _mk_dist(td, exes=("acsd", "helper"))
        r = _run_cli(["--legacy-check", "--dist", dist])
        check("N1_two_entries_red", r.returncode, 1, r.stdout + r.stderr, "非单一入口")

        shutil.rmtree(dist)
        _mk_dist(td, files=("VERSION", "LICENSE", "checksums.sha256"))
        r = _run_cli(["--legacy-check", "--dist", dist])
        check("N2_missing_required_red", r.returncode, 1, r.stdout + r.stderr, "missing README.txt")

        shutil.rmtree(dist)
        _mk_dist(td, bad_checksum=True)
        r = _run_cli(["--legacy-check", "--dist", dist])
        check("N3_checksum_mismatch_red", r.returncode, 1, r.stdout + r.stderr,
              "checksum mismatch VERSION")

        shutil.rmtree(dist)
        _mk_dist(td, banned=("testdata",))
        r = _run_cli(["--legacy-check", "--dist", dist])
        check("N4_banned_dir_red", r.returncode, 1, r.stdout + r.stderr, "不应含 testdata")

        # N5 锚缺失：dist 不存在 ⇒ 具名 ANCHOR_STALE（旧实现此处 rc=1 且零输出）
        shutil.rmtree(dist)
        r = _run_cli(["--legacy-check", "--dist", dist])
        check("N5_dist_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: DIST_REL")

    print("SELF_TEST %s cases=%d problems=%d"
          % ("PASS" if not problems else "FAIL", cases, len(problems)))
    for p in problems:
        print("  - " + p)
    return 0 if not problems else 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--legacy-check", action="store_true", dest="legacy_check",
                    help="复跑退役前的原判定实现（判据未改）")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--dist", default=None, help="发布布局目录（默认 <root>/%s）" % DIST_REL)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()
    if not args.legacy_check:
        print(RETIREMENT_MARKER, file=sys.stderr)
        print("  复跑原实现: --legacy-check [--root DIR] [--dist DIR]", file=sys.stderr)
        return 2
    dist = args.dist or os.path.join(os.path.abspath(args.root), DIST_REL)
    return legacy_main(dist)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(3)
