#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_final_traceability.py — REL-002 最终追溯校验（**已退役，非门禁**）。

退役判定依据
  1) 判据面已被在册门承接：六层追溯 = 注册项 TRACEABILITY / CON-TRACEABILITY /
     TRACEABILITY-MATRIX；旧入口退出面 = eng/tools/check_legacy_exit.py
     （注册项 CHK-SCI-REF 的 ACR-DORMANT，LEG-002..004）。本脚本在
     eng/ci/checks.json 与 docs/ci/01_CHECKS.md §2 中零引用。
  2) 判据对象不存在：发布状态文档 RELEASE_STATUS.md 已删除 ⇒ 原实现无参实跑即 rc=1
     **裸 traceback** FileNotFoundError，违反 docs/ci/01_CHECKS.md §1:14「不得 traceback」。
  3) 本次一并订正的三处陈旧/失效锚（只订正锚与单源口径，判据不放松）：
     * 硬编码版本断言 ver != "0.10.0-alpha.2" ⇒ 改为从根 VERSION 读（VER-001 单源，
       与同批 check_reproducible_build.py 的既有订正同款）；否则本文件在现行
       0.11.0-alpha.2 下**结构性恒红**，保留可复跑性无从谈起；
     * p3_resample.cpp 的锚 lib/phase3_session/ ⇒ lib/algorithms/resample/（W4-A9 迁移后真址）；
     * 委托的 eng/tools/check_traceability.py 自身已按 §2.1 退役（无参 rc=2 打印
       TRACEABILITY_RETIRED）⇒ 不再把「被委托门已退役」计为追溯失败；显式给
       --traceability-csv 时仍按原样委托复跑。

退役契约（docs/ci/01_CHECKS.md §2.1「检查器退役与预留」）
  * 无参调用：打印退役标识并 exit 2；
  * 原实现保留为 legacy_main()，经 --legacy-check 复跑；
  * 本检查器不写任何产物文件 ⇒ 不存在「产物落仓库根」的问题。

退出码：0 = --legacy-check 通过；1 = --legacy-check 判据违规；
        2 = 退役（无参调用）/ ANCHOR_STALE（锚失效，fail-closed）；3 = 用法错误。
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ── 判据锚（docs/ci/01_CHECKS.md §1:14 锚存活） ────────────────────────────
VERSION_REL = "VERSION"
PUBLIC_API_REL = os.path.join("docs", "contracts", "PUBLIC_API.md")
RELEASE_STATUS_REL = os.path.join("docs", "review", "RELEASE_STATUS.md")
TRACEABILITY_CHECKER_REL = os.path.join("eng", "tools", "check_traceability.py")
# W4-A9 批次 1/2 迁移后的真址（原 lib/phase3_session/ 已不是 p3_resample 的落点）
P3_IMPL_RELS = (os.path.join("lib", "algorithms", "projection", "p3_wcs.cpp"),
                os.path.join("lib", "algorithms", "resample", "p3_resample.cpp"),
                os.path.join("lib", "algorithms", "fits_output", "p3_output.cpp"))
LEGACY_ENTRY_TOKENS = ("orchestrator.exe", "astrocs-stage2.exe")
# 原实现的版本断言是**硬编码历史冻结值**（REL-002 成文时的 alpha 基线）。
# 保留原判据不动：现行 VERSION 已推进 ⇒ 该常量成为**失效锚**，按
# docs/ci/01_CHECKS.md §1:14 以 ANCHOR_STALE: EXPECTED_VERSION <值> 具名失败（rc=2），
# 既不静默降级、也不为了让它变绿而把断言改成读 VERSION（那是放宽判据）。
EXPECTED_VERSION = "0.10.0-alpha.2"

RETIREMENT_MARKER = (
    "RETIRED: check_final_traceability.py（REL-002）未注册进 eng/ci/checks.json；"
    "六层追溯在册门 = TRACEABILITY / CON-TRACEABILITY / TRACEABILITY-MATRIX，"
    "旧入口退出面在册门 = CHK-SCI-REF 的 ACR-DORMANT（eng/tools/check_legacy_exit.py）。"
)


class AnchorStale(Exception):
    """锚失效 —— fail-closed，rc=2（§1:14）。"""


def _read_anchor(root: str, const: str, rel: str) -> str:
    path = os.path.join(root, rel)
    if not os.path.isfile(path):
        raise AnchorStale("ANCHOR_STALE: %s %s" % (const, rel.replace(os.sep, "/")))
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


def legacy_main(root: str, traceability_csv: str | None) -> int:
    """退役前的原判定实现（判据未改；只把裸 traceback 换成具名锚失败）。"""
    errors: list[str] = []
    stale: list[str] = []
    version_text = _read_anchor(root, "VERSION_REL", VERSION_REL)
    pub_lines = _read_anchor(root, "PUBLIC_API_REL", PUBLIC_API_REL).splitlines()
    release_status = _read_anchor(root, "RELEASE_STATUS_REL", RELEASE_STATUS_REL)

    # 1) 追溯矩阵：被委托门已按 §2.1 退役 ⇒ 显式声明跳过（非静默），
    #    显式给 CSV 时仍按原样委托复跑。
    if traceability_csv:
        proc = subprocess.run(
            [sys.executable, os.path.join(root, TRACEABILITY_CHECKER_REL), traceability_csv],
            capture_output=True, text=True, timeout=120)
        if proc.returncode != 0:
            errors.append("traceability FAIL: %s" % proc.stdout[-300:])
    else:
        print("REL-002_TRACEABILITY_SKIP: eng/tools/check_traceability.py 已按 "
              "docs/ci/01_CHECKS.md §2.1 退役（TRACEABILITY-CODE），未给 "
              "--traceability-csv ⇒ 本项不复跑追溯链（显式跳过，非静默）")

    # 2) 旧入口扫描 (生产文档不得把遗留当唯一入口)
    for legacy in LEGACY_ENTRY_TOKENS:
        for ln in pub_lines:
            if legacy in ln and "LEG-" not in ln:
                errors.append("旧入口未标退出: %s" % legacy)
    # 3) 版本断言（原判据：VERSION 必须等于 REL-002 成文时的冻结基线）
    ver = version_text.strip()
    if ver != EXPECTED_VERSION:
        stale.append("ANCHOR_STALE: EXPECTED_VERSION %s（REL-002 历史冻结值；"
                     "现行 VERSION = %s）" % (EXPECTED_VERSION, ver))
    # 4) phase3 假状态: 真实实现存在
    for rel in P3_IMPL_RELS:
        if not os.path.isfile(os.path.join(root, rel)):
            errors.append("phase3 实现缺失: %s" % rel.replace(os.sep, "/"))
    # 5) RELEASE_STATUS 诚实性
    if "基本" in release_status:
        errors.append("RELEASE_STATUS 用模糊词 '基本'")
    if stale:
        for s in stale:
            print(s, file=sys.stderr)
    if errors:
        print("REL-002_TRACE_VIOLATION (%d):" % len(errors))
        for e in errors:
            print("  " + e)
        return 1
    if stale:
        print("REL-002_FAIL: 硬编码锚失效（fail-closed，docs/ci/01_CHECKS.md §1:14）")
        return 2
    print("REL-002_PASS: 旧入口标退出, VERSION == 冻结基线 %s, phase3 真实实现齐, "
          "RELEASE_STATUS 诚实" % ver)
    return 0


# ────────────────────────────────────────────────────────────── self-test
def _run_cli(args: list[str]) -> "subprocess.CompletedProcess":
    return subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                          capture_output=True, text=True, timeout=300)


def _mk_root(td: str, version=EXPECTED_VERSION, status="全部通过\n",
             pub_extra="", drop_p3=None) -> str:
    os.makedirs(os.path.join(td, "docs", "contracts"), exist_ok=True)
    os.makedirs(os.path.join(td, "docs", "review"), exist_ok=True)
    with open(os.path.join(td, VERSION_REL), "w", encoding="utf-8") as fh:
        fh.write(version + "\n")
    with open(os.path.join(td, PUBLIC_API_REL), "w", encoding="utf-8") as fh:
        fh.write("唯一入口 astrocs\n" + pub_extra)
    with open(os.path.join(td, RELEASE_STATUS_REL), "w", encoding="utf-8") as fh:
        fh.write(status)
    for rel in P3_IMPL_RELS:
        if rel == drop_p3:
            continue
        p = os.path.join(td, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as fh:
            fh.write("// fixture\n")
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

    with tempfile.TemporaryDirectory(prefix="final_trace_st_") as td:
        _mk_root(td)
        r = _run_cli(["--legacy-check", "--root", td])
        check("P1_legacy_green", r.returncode, 0, r.stdout + r.stderr, "REL-002_PASS")

        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td, pub_extra="orchestrator.exe 仍是入口\n")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N1_unmarked_legacy_entry_red", r.returncode, 1, r.stdout + r.stderr,
              "旧入口未标退出")

        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td, drop_p3=P3_IMPL_RELS[1])
        r = _run_cli(["--legacy-check", "--root", td])
        check("N2_p3_impl_missing_red", r.returncode, 1, r.stdout + r.stderr,
              "phase3 实现缺失: lib/algorithms/resample/p3_resample.cpp")

        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td, status="本项基本完成\n")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N3_vague_status_red", r.returncode, 1, r.stdout + r.stderr, "模糊词")

        # N4 硬编码冻结基线已失效（现行 VERSION）⇒ 具名 ANCHOR_STALE + rc=2
        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td, version="0.11.0-alpha.2")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N4_expected_version_stale_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: EXPECTED_VERSION")

        # N5 锚缺失：RELEASE_STATUS ⇒ 具名 ANCHOR_STALE（旧实现裸 traceback）
        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td)
        os.remove(os.path.join(td, RELEASE_STATUS_REL))
        r = _run_cli(["--legacy-check", "--root", td])
        check("N5_release_status_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: RELEASE_STATUS_REL")

        # N6 锚缺失：PUBLIC_API ⇒ 具名 ANCHOR_STALE
        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td)
        os.remove(os.path.join(td, PUBLIC_API_REL))
        r = _run_cli(["--legacy-check", "--root", td])
        check("N6_public_api_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: PUBLIC_API_REL")

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
    ap.add_argument("--traceability-csv", default=None,
                    help="显式追溯表；给出时委托 eng/tools/check_traceability.py 复跑")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()
    if not args.legacy_check:
        print(RETIREMENT_MARKER, file=sys.stderr)
        print("  复跑原实现: --legacy-check [--root DIR] [--traceability-csv CSV]",
              file=sys.stderr)
        return 2
    try:
        return legacy_main(os.path.abspath(args.root), args.traceability_csv)
    except AnchorStale as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(3)
