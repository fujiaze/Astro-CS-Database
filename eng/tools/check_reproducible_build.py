#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_reproducible_build.py — QA-005 可复现构建依赖与 SBOM 校验（**已退役，非门禁**）。

退役判定依据
  1) 判据面已被在册门承接：SBOM 面 = CHK-PKG-CONSISTENCY 的 PKG-SBOM / PKG-SBOM-NEG；
     build id / 版本单源生成链 = eng/ci/check_version.py 的生成链判据（注册项
     VERSION-CONSISTENCY）。本脚本在 eng/ci/checks.json 与 docs/ci/01_CHECKS.md §2 中零引用。
  2) 判据对象不存在：两个回退锚 dist/astrocs-alpha 与
     evidence/v6_1_rework/tasks/QA-003/SBOM.json 均已删除 ⇒ 无参实跑 rc=1
     QA-005_VIOLATION: SBOM.json 缺失（恒红，且红的理由是对象不存在而非缺陷）。
  3) 本次一并订正的 fail-open / traceback：
     * 原 find_cli_bin() 返回 None 时**静默跳过** build-id 与复现性两段（§1:13 禁止静默降级）
       ⇒ 改为具名 ANCHOR_STALE + rc=2；
     * VERSION 缺失原为 FileNotFoundError 裸 traceback（§1:14 禁止 traceback）
       ⇒ 改为具名 ANCHOR_STALE + rc=2。

退役契约（docs/ci/01_CHECKS.md §2.1「检查器退役与预留」）
  * 无参调用：打印退役标识并 exit 2；
  * 原实现保留为 legacy_main()，经 --legacy-check 复跑（判据未改）；
  * 本检查器不写任何产物文件 ⇒ 不存在「产物落仓库根」的问题。

退出码：0 = --legacy-check 通过；1 = --legacy-check 判据违规；
        2 = 退役（无参调用）/ ANCHOR_STALE（锚失效，fail-closed）；3 = 用法错误。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ── 判据锚（docs/ci/01_CHECKS.md §1:14 锚存活） ────────────────────────────
VERSION_REL = "VERSION"
DEPS_REL = "DEPENDENCIES.md"
DIST_SBOM_REL = os.path.join("dist", "astrocs-alpha", "SBOM.json")
SBOM_FALLBACK_REL = os.path.join("evidence", "v6_1_rework", "tasks", "QA-003", "SBOM.json")
CLI_BIN_CANDIDATES = (os.path.join("build", "root-cmake", "acsd"),
                      os.path.join("build", "acsd"),
                      os.path.join("build", "cli", "acsd"))
SEMVER_ALPHA = re.compile(r"^\d+\.\d+\.\d+-alpha\.\d+$")

RETIREMENT_MARKER = (
    "RETIRED: check_reproducible_build.py（QA-005）未注册进 eng/ci/checks.json；"
    "SBOM 在册门 = CHK-PKG-CONSISTENCY 的 PKG-SBOM/PKG-SBOM-NEG，"
    "build id/版本单源在册门 = VERSION-CONSISTENCY（eng/ci/check_version.py）。"
)


def read_base_version(root: str) -> str:
    path = os.path.join(root, VERSION_REL)
    if not os.path.isfile(path):
        raise AnchorStale("ANCHOR_STALE: VERSION_REL %s" % VERSION_REL)
    with open(path, encoding="utf-8") as fh:
        raw = fh.read().strip()
    if not SEMVER_ALPHA.match(raw):
        raise AnchorStale("ANCHOR_STALE: VERSION_REL %s 内容非 X.Y.Z-alpha.N: %r"
                          % (VERSION_REL, raw))
    return raw


class AnchorStale(Exception):
    """锚失效 / 扫描面塌缩 —— fail-closed，rc=2（§1:13–14）。"""


def find_cli_bin(root: str, explicit: str | None) -> str:
    if explicit:
        if not os.path.isfile(explicit):
            raise AnchorStale("ANCHOR_STALE: CLI_BIN_ARG %s" % explicit)
        return explicit
    for rel in CLI_BIN_CANDIDATES:
        p = os.path.join(root, rel)
        if os.path.exists(p):
            return p
    raise AnchorStale("ANCHOR_STALE: CLI_BIN_CANDIDATES %s（候选全不存在，"
                      "禁止静默跳过 build-id 与复现性判据）"
                      % ", ".join(c.replace(os.sep, "/") for c in CLI_BIN_CANDIDATES))


def load_sbom(root: str):
    """dist 发布 SBOM 优先; 缺失回退 evidence QA-003 SBOM 证据。"""
    dist = os.path.join(root, DIST_SBOM_REL)
    fb = os.path.join(root, SBOM_FALLBACK_REL)
    if os.path.isfile(dist):
        return dist, False
    if os.path.isfile(fb):
        return fb, True
    return None, False


def legacy_main(root: str, cli_bin: str | None) -> int:
    """退役前的原判定实现（判据未改；只补具名锚检查与 fail-closed）。"""
    errors: list[str] = []
    try:
        base = read_base_version(root)
        bin_path = find_cli_bin(root, cli_bin)
    except AnchorStale as exc:
        print(str(exc), file=sys.stderr)
        return 2

    # 1) 工具链版本
    if not os.path.isfile(os.path.join(root, DEPS_REL)):
        errors.append("%s 缺失 (工具链版本锁定)" % DEPS_REL)
    # 2) build id 可追溯 (版本单源: 根 VERSION, VER-001)
    proc = subprocess.run([str(bin_path), "--version"], capture_output=True,
                          text=True, timeout=60)
    ver = proc.stdout.strip()
    if not re.search(re.escape(base) + r"\+g[0-9a-f]{7,}", ver):
        errors.append("build id 不可追溯 (期望 %s+g<commit>): %s" % (base, ver))
    # 3) SBOM
    sbom_path, sbom_fallback = load_sbom(root)
    if sbom_path is not None:
        with open(sbom_path, encoding="utf-8", errors="replace") as fh:
            doc = json.loads(fh.read())
        if not doc.get("components"):
            errors.append("SBOM 组件空")
        comp = doc.get("metadata", {}).get("component", {})
        if not doc.get("license") and not comp.get("licenses"):
            errors.append("SBOM license 缺失")
        comp_ver = comp.get("version")
        if sbom_fallback:
            # 历史 evidence SBOM = 构建时点快照: 只要求 SBOM 自洽非空
            if comp_ver and doc.get("version") not in (None, 1) and doc.get("version") != comp_ver:
                errors.append("SBOM version 自洽失败: %s != %s" % (doc.get("version"), comp_ver))
        else:
            sbom_ver = doc.get("version") if isinstance(doc.get("version"), str) else comp_ver
            if sbom_ver != base:
                errors.append("SBOM version 不一致: %s != %s" % (sbom_ver, base))
    else:
        errors.append("SBOM.json 缺失")
    # 4) 复现性: 同二进制两次 --version 一致
    proc2 = subprocess.run([str(bin_path), "--version"], capture_output=True,
                           text=True, timeout=60)
    if proc2.stdout.strip() != ver:
        errors.append("--version 两次输出不一致: %r vs %r" % (ver, proc2.stdout.strip()))
    if errors:
        print("QA-005_VIOLATION (%d):" % len(errors))
        for e in errors:
            print("  " + e)
        return 1
    sbom_note = ("SBOM 完整 (evidence 回退: %s)"
                 % SBOM_FALLBACK_REL.replace(os.sep, "/")) if sbom_fallback else "SBOM 完整"
    print("QA-005_PASS: 工具链锁定, build id 可追溯 (%s+g<commit>), %s" % (base, sbom_note))
    return 0


# ────────────────────────────────────────────────────────────── self-test
def _run_cli(args: list[str]) -> "subprocess.CompletedProcess":
    return subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                          capture_output=True, text=True, timeout=300)


def _fake_version_bin(version_line: str) -> str:
    return "#!%s\nimport sys\nsys.stdout.write(%r)\n" % (sys.executable, version_line)


def _mk_root(td: str, version="0.11.0-alpha.2", bin_line=None, sbom="ok",
             deps=True) -> str:
    if deps:
        with open(os.path.join(td, DEPS_REL), "w", encoding="utf-8") as fh:
            fh.write("gcc 13\n")
    with open(os.path.join(td, VERSION_REL), "w", encoding="utf-8") as fh:
        fh.write(version + "\n")
    binp = os.path.join(td, CLI_BIN_CANDIDATES[1])
    os.makedirs(os.path.dirname(binp), exist_ok=True)
    line = bin_line if bin_line is not None else ("acsd %s+g1234567890ab\n" % version)
    with open(binp, "w", encoding="utf-8") as fh:
        fh.write(_fake_version_bin(line))
    os.chmod(binp, 0o755)
    if sbom == "ok":
        dist = os.path.join(td, "dist", "astrocs-alpha")
        os.makedirs(dist, exist_ok=True)
        with open(os.path.join(dist, "SBOM.json"), "w", encoding="utf-8") as fh:
            json.dump({"version": version, "components": [{"name": "zlib"}],
                       "metadata": {"component": {"version": version,
                                                  "licenses": [{"license": "MIT"}]}}}, fh)
    elif sbom == "version_mismatch":
        dist = os.path.join(td, "dist", "astrocs-alpha")
        os.makedirs(dist, exist_ok=True)
        with open(os.path.join(dist, "SBOM.json"), "w", encoding="utf-8") as fh:
            json.dump({"version": "9.9.9", "components": [{"name": "zlib"}],
                       "license": "MIT",
                       "metadata": {"component": {"version": "9.9.9"}}}, fh)
    elif sbom == "empty_components":
        dist = os.path.join(td, "dist", "astrocs-alpha")
        os.makedirs(dist, exist_ok=True)
        with open(os.path.join(dist, "SBOM.json"), "w", encoding="utf-8") as fh:
            json.dump({"version": version, "components": [], "license": "MIT",
                       "metadata": {"component": {"version": version}}}, fh)
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

    with tempfile.TemporaryDirectory(prefix="repro_st_") as td:
        _mk_root(td)
        r = _run_cli(["--legacy-check", "--root", td])
        check("P1_legacy_green", r.returncode, 0, r.stdout + r.stderr, "QA-005_PASS")

        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td, bin_line="acsd 0.11.0-alpha.2\n")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N1_build_id_untraceable_red", r.returncode, 1, r.stdout + r.stderr,
              "build id 不可追溯")

        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td, sbom="version_mismatch")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N2_sbom_version_red", r.returncode, 1, r.stdout + r.stderr,
              "SBOM version 不一致")

        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td, sbom="empty_components")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N3_sbom_components_empty_red", r.returncode, 1, r.stdout + r.stderr,
              "SBOM 组件空")

        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td, sbom="none")
        r = _run_cli(["--legacy-check", "--root", td])
        check("N4_sbom_missing_red", r.returncode, 1, r.stdout + r.stderr, "SBOM.json 缺失")

        # N5 VERSION 缺失 ⇒ 具名 ANCHOR_STALE（旧实现裸 traceback）
        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td)
        os.remove(os.path.join(td, VERSION_REL))
        r = _run_cli(["--legacy-check", "--root", td])
        check("N5_version_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: VERSION_REL")

        # N6 CLI 二进制候选全缺 ⇒ 具名 ANCHOR_STALE（旧实现静默跳过两段判据）
        shutil.rmtree(td)
        os.makedirs(td)
        _mk_root(td)
        os.remove(os.path.join(td, CLI_BIN_CANDIDATES[1]))
        r = _run_cli(["--legacy-check", "--root", td])
        check("N6_cli_bin_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: CLI_BIN_CANDIDATES")

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
    ap.add_argument("--cli-bin", default=None, help="显式 CLI 二进制（默认按候选表探测）")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()
    if not args.legacy_check:
        print(RETIREMENT_MARKER, file=sys.stderr)
        print("  复跑原实现: --legacy-check [--root DIR] [--cli-bin PATH]", file=sys.stderr)
        return 2
    return legacy_main(os.path.abspath(args.root), args.cli_bin)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(3)
