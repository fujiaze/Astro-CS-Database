#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_release_consistency.py — G11 发布一致性校验（**已退役，非门禁**）。

退役判定依据
  1) 判据面已被在册门承接：VERSION 单源语义与生成链在 eng/ci/check_version.py
     （注册项 VERSION-CONSISTENCY）中；发布布局 / checksums / SBOM 在
     eng/ci/checks.json 的 CHK-PKG-CONSISTENCY（PKG-CONSISTENCY / PKG-SBOM 及 -NEG）
     中。本脚本在 eng/ci/checks.json 与 docs/ci/01_CHECKS.md §2 中零引用。
  2) 判据对象不存在：dist/astrocs-alpha 与发布状态文档 RELEASE_STATUS.md 均已删除；
     原实现对后者**裸读**，无参实跑即 rc=1 裸 traceback FileNotFoundError，
     违反 docs/ci/01_CHECKS.md §1:14「不得 traceback」。
  3) 本次一并订正：RELEASE_STATUS 锚缺失 ⇒ 具名 ANCHOR_STALE + rc=2（不再 traceback）。

退役契约（docs/ci/01_CHECKS.md §2.1「检查器退役与预留」）
  * 无参调用：打印退役标识并 exit 2；
  * 原实现保留为 legacy_main()，经 --legacy-check 复跑（判据未改）；
  * 本检查器不写任何产物文件 ⇒ 不存在「产物落仓库根」的问题。

退出码：0 = --legacy-check 通过；1 = --legacy-check 判据违规；
        2 = 退役（无参调用）/ ANCHOR_STALE（锚失效，fail-closed）；3 = 用法错误。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ── 判据锚（docs/ci/01_CHECKS.md §1:14 锚存活） ────────────────────────────
VERSION_REL = "VERSION"
DIST_REL = os.path.join("dist", "astrocs-alpha")
RELEASE_STATUS_REL = os.path.join("docs", "review", "RELEASE_STATUS.md")
# semver 主干 + 可选预发布段 (0.11.0-alpha.2 等)
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?$")

RETIREMENT_MARKER = (
    "RETIRED: check_release_consistency.py（G11）未注册进 eng/ci/checks.json；"
    "VERSION 单源语义在册门 = VERSION-CONSISTENCY（eng/ci/check_version.py），"
    "发布布局/checksums/SBOM 在册门 = CHK-PKG-CONSISTENCY（PKG-CONSISTENCY/PKG-SBOM）。"
)


def legacy_main(root: str) -> int:
    """退役前的原判定实现（判据未改；只把裸读改成具名锚检查）。"""
    errors: list[str] = []
    version_path = os.path.join(root, VERSION_REL)
    if not os.path.isfile(version_path):
        print("ANCHOR_STALE: VERSION_REL %s" % VERSION_REL, file=sys.stderr)
        return 2
    release_status = os.path.join(root, RELEASE_STATUS_REL)
    if not os.path.isfile(release_status):
        print("ANCHOR_STALE: RELEASE_STATUS_REL %s"
              % RELEASE_STATUS_REL.replace(os.sep, "/"), file=sys.stderr)
        return 2

    # 1) VERSION 单源语义校验 (存在/非空/格式合法; 不比较硬编码版本)
    with open(version_path, encoding="utf-8", errors="ignore") as fh:
        ver = fh.read().strip()
    if not ver:
        errors.append("VERSION 文件为空")
    elif not VERSION_RE.match(ver):
        errors.append("VERSION 格式异常 (需 ^\\d+\\.\\d+\\.\\d+(-[0-9A-Za-z.]+)?$): %s" % ver)
    # 2) 发布布局 checksums
    dist = os.path.join(root, DIST_REL)
    if os.path.isdir(dist):
        cs = os.path.join(dist, "checksums.sha256")
        if os.path.isfile(cs):
            with open(cs, encoding="utf-8", errors="replace") as fh:
                for line in fh.read().strip().splitlines():
                    parts = line.split()
                    if len(parts) != 2:
                        errors.append("checksums 行格式非法: %r" % line.strip()[:80])
                        continue
                    h, name = parts
                    target = os.path.join(dist, name)
                    if not os.path.isfile(target):
                        errors.append("checksum 指向不存在文件: %s" % name)
                        continue
                    with open(target, "rb") as bf:
                        real = hashlib.sha256(bf.read()).hexdigest()
                    if h != real:
                        errors.append("checksum mismatch: %s" % name)
    # 3) SBOM
    sbom = os.path.join(dist, "SBOM.json")
    if os.path.isfile(sbom):
        with open(sbom, encoding="utf-8", errors="replace") as fh:
            doc = json.loads(fh.read())
        if doc.get("product") != "AstroCS":
            errors.append("SBOM product 异常")
        if doc.get("version") != ver:
            errors.append("SBOM version %s != %s" % (doc.get("version"), ver))
    # 4) NOT VERIFIED 扫描
    with open(release_status, encoding="utf-8", errors="ignore") as fh:
        rs = fh.read()
    if "NOT VERIFIED" in rs and "owner" not in rs.lower():
        errors.append("RELEASE_STATUS 含未登记 NOT VERIFIED")
    if errors:
        print("REL_CONSISTENCY_VIOLATION (%d):" % len(errors))
        for e in errors:
            print("  " + e)
        return 1
    print("REL_CONSISTENCY_PASS: VERSION 单源语义合法, checksums 一致, SBOM 对齐, 无未登记 NOT VERIFIED")
    return 0


# ────────────────────────────────────────────────────────────── self-test
def _run_cli(args: list[str]) -> "subprocess.CompletedProcess":
    import subprocess
    return subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                          capture_output=True, text=True, timeout=300)


def _mk_root(td: str, version="0.11.0-alpha.2", status_text="全部通过\n",
             sbom_version=None, bad_checksum=False) -> str:
    os.makedirs(os.path.join(td, "docs", "review"), exist_ok=True)
    with open(os.path.join(td, VERSION_REL), "w", encoding="utf-8") as fh:
        fh.write(version + "\n")
    with open(os.path.join(td, RELEASE_STATUS_REL), "w", encoding="utf-8") as fh:
        fh.write(status_text)
    if sbom_version is not None or bad_checksum:
        dist = os.path.join(td, DIST_REL)
        os.makedirs(dist, exist_ok=True)
        with open(os.path.join(dist, "payload.bin"), "wb") as fh:
            fh.write(b"payload\n")
        with open(os.path.join(dist, "payload.bin"), "rb") as bf:
            digest = hashlib.sha256(bf.read()).hexdigest()
        if bad_checksum:
            digest = "0" * 64
        with open(os.path.join(dist, "checksums.sha256"), "w", encoding="utf-8") as fh:
            fh.write("%s  payload.bin\n" % digest)
        if sbom_version is not None:
            with open(os.path.join(dist, "SBOM.json"), "w", encoding="utf-8") as fh:
                json.dump({"product": "AstroCS", "version": sbom_version}, fh)
    return td


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

    with tempfile.TemporaryDirectory(prefix="rel_cons_st_") as td:
        _mk_root(td)
        r = _run_cli(["--legacy-check", "--root", td])
        check("P1_legacy_green", r.returncode, 0, r.stdout + r.stderr, "REL_CONSISTENCY_PASS")

        shutil.rmtree(td)
        _mk_root(td, version="not-a-version")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N1_version_format_red", r.returncode, 1, r.stdout + r.stderr, "VERSION 格式异常")

        shutil.rmtree(td)
        _mk_root(td, status_text="X: NOT VERIFIED\n")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N2_unregistered_not_verified_red", r.returncode, 1, r.stdout + r.stderr,
              "含未登记 NOT VERIFIED")

        shutil.rmtree(td)
        _mk_root(td, sbom_version="9.9.9")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N3_sbom_version_red", r.returncode, 1, r.stdout + r.stderr, "SBOM version")

        shutil.rmtree(td)
        _mk_root(td, bad_checksum=True)
        r = _run_cli(["--legacy-check", "--root", td])
        check("N4_checksum_mismatch_red", r.returncode, 1, r.stdout + r.stderr,
              "checksum mismatch")

        # N5 锚缺失：RELEASE_STATUS 不存在 ⇒ 具名 ANCHOR_STALE（旧实现此处裸 traceback）
        shutil.rmtree(td)
        _mk_root(td)
        os.remove(os.path.join(td, RELEASE_STATUS_REL))
        r = _run_cli(["--legacy-check", "--root", td])
        check("N5_release_status_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: RELEASE_STATUS_REL")

        # N6 锚缺失：VERSION 不存在 ⇒ ANCHOR_STALE rc=2
        shutil.rmtree(td)
        _mk_root(td)
        os.remove(os.path.join(td, VERSION_REL))
        r = _run_cli(["--legacy-check", "--root", td])
        check("N6_version_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: VERSION_REL")

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
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()
    if not args.legacy_check:
        print(RETIREMENT_MARKER, file=sys.stderr)
        print("  复跑原实现: --legacy-check [--root DIR]", file=sys.stderr)
        return 2
    return legacy_main(os.path.abspath(args.root))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(3)
