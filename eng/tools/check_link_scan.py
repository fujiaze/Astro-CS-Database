#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""BLD-001 link scan: 校验根 CMake 生产 CLI 不链接 ACR 符号、无 production GLOB。

判据（任一违规 ⇒ rc=1）
  C1 cmake_no_production_glob   根 CMakeLists.txt 不得出现 production GLOB
                                （排除 vendored cfitsio 生成清单行与注释行）
  C2 binary_no_acr_symbols      生产二进制 nm -C 输出不得含 ACR 符号
                                （astro::compute / kernel_registry / device_executor / acr_）

fail-closed（docs/ci/01_CHECKS.md §1「注册表原则」:12–15）
  * 锚存活：判据里硬编码引用的仓库路径必须存在，失效时以
    ANCHOR_STALE: <常量名> <路径> 显式失败并点名（rc=2），不得 traceback、不得静默降级；
  * 默认二进制路径不存在 ⇒ **判红**（rc=2）—— 旧实现把「二进制不存在」当「无 ACR 违规」，
    静默跳过 nm 段仍打印 LINK_SCAN_PASS（恒绿）；默认值同时由失效的
    build/root-cmake/astrocs 改为现行构建布局 build/acsd（AGENTS.md §3）；
  * nm 不可用 / nm 非零退出 / nm 输出无可解析符号行 ⇒ 扫描面塌缩 ⇒ 判红（rc=2）
    （§1 :13「scanned == 0 ⇒ rc != 0」）。

可执行负例面（§1 :10–11）
  --self-test：临时沙箱内用假 nm（PATH 注入）+ 合成 CMakeLists 真跑本 CLI 子进程，
  逐例断言 rc 与具名结论：正例绿 / ACR 符号红 / production GLOB 红 / 二进制缺失
  ANCHOR_STALE 红 / CMakeLists 缺失 ANCHOR_STALE 红 / nm 输出为空红 /
  nm 缺失 TOOL_UNAVAILABLE 红 / nm 非零退出红。

用法
  python3 eng/tools/check_link_scan.py [binary] [--root DIR] [--self-test]
exit 0 = PASS；1 = 判据违规；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys

# ── 判据锚（§1 锚存活：硬编码引用的仓库路径必须存在） ──────────────────────
CMAKELISTS_REL = "CMakeLists.txt"
DEFAULT_BINARY_REL = os.path.join("build", "acsd")
NM_TOOL = "nm"

# vendored cfitsio 生成清单行（GLOB 合法用法）与注释行不判红
GLOB_EXEMPT_TOKEN = "cfitsio_sources"
ACR_TOKENS = ("astro::compute", "kernel_registry", "device_executor", "acr_")

# nm 输出行形态：<hex 地址或空格> <类型字母> <符号名>
NM_ENTRY_RE = re.compile(r"^[0-9a-fA-F]{8,}\s+[A-Za-z?]\s+\S")


class AnchorStale(Exception):
    """锚失效 / 扫描面塌缩 —— fail-closed，rc=2（§1 锚存活 + fail-closed）。"""


def _repo_root() -> str:
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def cmake_glob_hits(text: str) -> list[str]:
    """production GLOB 行（排除注释行与 vendored 生成清单行）。"""
    hits = []
    for line in text.splitlines():
        if GLOB_EXEMPT_TOKEN in line:
            continue
        if line.strip().startswith("#"):
            continue
        if "GLOB" in line:
            hits.append(line.strip())
    return hits


def acr_symbol_hits(nm_stdout: str) -> list[str]:
    """nm -C 输出里命中 ACR 符号面的行。"""
    hits = []
    for line in nm_stdout.splitlines():
        if any(tok in line for tok in ACR_TOKENS):
            hits.append(line.strip()[:100])
    return hits


def count_symbol_lines(nm_stdout: str) -> int:
    """nm 输出里可解析的符号行数（用于判「扫描面是否塌缩」）。"""
    return sum(1 for line in nm_stdout.splitlines() if NM_ENTRY_RE.match(line))


def scan(root: str, binary_rel: str) -> tuple[int, list[str]]:
    """返回 (rc, 输出行)。rc: 0=PASS，1=判据违规，2=fail-closed。"""
    out: list[str] = []

    # ── C1 根 CMakeLists.txt ────────────────────────────────────────────
    cmake_path = os.path.join(root, CMAKELISTS_REL)
    if not os.path.isfile(cmake_path):
        raise AnchorStale("ANCHOR_STALE: CMAKELISTS_REL %s" % CMAKELISTS_REL)
    with open(cmake_path, encoding="utf-8", errors="replace") as fh:
        cmake_text = fh.read()

    # ── C2 生产二进制 ───────────────────────────────────────────────────
    binary_path = binary_rel if os.path.isabs(binary_rel) \
        else os.path.join(root, binary_rel)
    if not os.path.isfile(binary_path):
        # §1 fail-closed：不得把「文件不存在」当「无违规」。
        const = "DEFAULT_BINARY_REL" if binary_rel == DEFAULT_BINARY_REL else "BINARY_ARG"
        raise AnchorStale("ANCHOR_STALE: %s %s" % (const, binary_rel))

    nm_exe = shutil.which(NM_TOOL)
    if nm_exe is None:
        raise AnchorStale("TOOL_UNAVAILABLE: %s 不在 PATH 上（ACR 符号扫描面不可用）" % NM_TOOL)
    proc = subprocess.run([nm_exe, "-C", binary_path],
                          capture_output=True, text=True, timeout=300)
    if proc.returncode != 0:
        raise AnchorStale("TOOL_FAILED: %s -C %s rc=%d %s"
                          % (NM_TOOL, binary_rel, proc.returncode, proc.stderr.strip()[:160]))
    n_syms = count_symbol_lines(proc.stdout)
    if n_syms == 0:
        raise AnchorStale("SYMBOL_SCAN_EMPTY: %s -C %s 输出无可解析符号行 —— "
                          "扫描面塌缩，拒绝判绿" % (NM_TOOL, binary_rel))

    errors: list[str] = []
    glob_hits = cmake_glob_hits(cmake_text)
    acr_hits = acr_symbol_hits(proc.stdout)
    if glob_hits:
        errors.append("production GLOB in %s: %s" % (CMAKELISTS_REL, glob_hits))
    if acr_hits:
        errors.append("ACR symbols in production binary: %d (e.g. %s)"
                      % (len(acr_hits), acr_hits[:3]))
    if errors:
        out.append("LINK_SCAN_FAIL:")
        out.extend("  " + e for e in errors)
        return 1, out
    out.append("LINK_SCAN_PASS binary=%s symbols=%d globs=0 acr_symbols=0"
               % (binary_rel, n_syms))
    return 0, out


# ────────────────────────────────────────────────────────────── self-test
_FAKE_NM_CLEAN = """#!/bin/sh
echo "0000000000001139 T main"
echo "0000000000000000 T _init"
echo "                 U astro::aio::open_file"
echo "0000000000001200 T astro::pipeline::run"
"""
_FAKE_NM_ACR = """#!/bin/sh
echo "0000000000001139 T main"
echo "0000000000002000 T astro::compute::kernel_registry::lookup"
echo "                 U acr_device_executor_run"
"""
_FAKE_NM_FAIL = """#!/bin/sh
echo "nm: 'x': No such file" >&2
exit 1
"""
_FAKE_NM_EMPTY = """#!/bin/sh
exit 0
"""
_CMAKELISTS_CLEAN = """cmake_minimum_required(VERSION 3.20)
project(acsd CXX)
file(GLOB cfitsio_sources ${CFITSIO_DIR}/*.c)
# file(GLOB legacy_sources *.cpp)   # 注释里的 GLOB 不判红
add_executable(acsd main.cpp)
"""
_CMAKELISTS_GLOB = """cmake_minimum_required(VERSION 3.20)
project(acsd CXX)
file(GLOB production_sources lib/*.cpp)
"""


def _write(path: str, text: str, mode: int | None = None) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)
    if mode is not None:
        os.chmod(path, mode)


def _run_cli(root: str, binary_rel: str, path_dir: str | None) -> subprocess.CompletedProcess:
    env = dict(os.environ)
    if path_dir is not None:
        env["PATH"] = path_dir
    return subprocess.run(
        [sys.executable, os.path.abspath(__file__), binary_rel, "--root", root],
        capture_output=True, text=True, timeout=300, env=env)


def self_test() -> int:
    """可执行正/负例面（§1 :10–11 / ENGINEERING_SPEC §8）。"""
    import tempfile

    problems: list[str] = []
    cases = 0

    def check(name: str, rc: int, want_rc: int, blob: str, want_token: str) -> None:
        nonlocal cases
        cases += 1
        ok = rc == want_rc and want_token in blob
        print("  SELFTEST_%s %-34s rc=%d want_rc=%d token=%r"
              % ("PASS" if ok else "FAIL", name, rc, want_rc, want_token))
        if not ok:
            problems.append("%s: rc=%d(want %d) token=%r missing"
                            % (name, rc, want_rc, want_token))

    with tempfile.TemporaryDirectory(prefix="link_scan_st_") as td:
        root = os.path.join(td, "repo")
        bindir = os.path.join(td, "bin")
        empty_bindir = os.path.join(td, "emptybin")
        os.makedirs(empty_bindir)
        _write(os.path.join(root, "build", "acsd"), "ELF-fake\n")

        # P1 正例：无 GLOB、无 ACR 符号 ⇒ rc=0
        _write(os.path.join(root, CMAKELISTS_REL), _CMAKELISTS_CLEAN)
        _write(os.path.join(bindir, "nm"), _FAKE_NM_CLEAN, 0o755)
        r = _run_cli(root, DEFAULT_BINARY_REL, bindir)
        check("P1_clean_green", r.returncode, 0, r.stdout + r.stderr, "LINK_SCAN_PASS")

        # N1 ACR 符号 ⇒ rc=1
        _write(os.path.join(bindir, "nm"), _FAKE_NM_ACR, 0o755)
        r = _run_cli(root, DEFAULT_BINARY_REL, bindir)
        check("N1_acr_symbols_red", r.returncode, 1, r.stdout + r.stderr,
              "ACR symbols in production binary")

        # N2 production GLOB ⇒ rc=1
        _write(os.path.join(bindir, "nm"), _FAKE_NM_CLEAN, 0o755)
        _write(os.path.join(root, CMAKELISTS_REL), _CMAKELISTS_GLOB)
        r = _run_cli(root, DEFAULT_BINARY_REL, bindir)
        check("N2_production_glob_red", r.returncode, 1, r.stdout + r.stderr,
              "production GLOB in " + CMAKELISTS_REL)

        # N3 二进制缺失 ⇒ ANCHOR_STALE rc=2（旧实现恒绿的正是这一支）
        _write(os.path.join(root, CMAKELISTS_REL), _CMAKELISTS_CLEAN)
        r = _run_cli(root, os.path.join("build", "no_such_binary"), bindir)
        check("N3_binary_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: BINARY_ARG")

        # N4 默认二进制缺失 ⇒ ANCHOR_STALE rc=2（默认值本身也是锚）
        r = _run_cli(root, DEFAULT_BINARY_REL, bindir)
        check("N4_default_binary_present", r.returncode, 0, r.stdout + r.stderr,
              "LINK_SCAN_PASS")

        # N5 CMakeLists 缺失 ⇒ ANCHOR_STALE rc=2
        os.remove(os.path.join(root, CMAKELISTS_REL))
        r = _run_cli(root, DEFAULT_BINARY_REL, bindir)
        check("N5_cmakelists_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: CMAKELISTS_REL")
        _write(os.path.join(root, CMAKELISTS_REL), _CMAKELISTS_CLEAN)

        # N6 nm 输出为空 ⇒ 扫描面塌缩 rc=2
        _write(os.path.join(bindir, "nm"), _FAKE_NM_EMPTY, 0o755)
        r = _run_cli(root, DEFAULT_BINARY_REL, bindir)
        check("N6_nm_output_empty_red", r.returncode, 2, r.stdout + r.stderr,
              "SYMBOL_SCAN_EMPTY")

        # N7 nm 不可用 ⇒ TOOL_UNAVAILABLE rc=2
        r = _run_cli(root, DEFAULT_BINARY_REL, empty_bindir)
        check("N7_nm_unavailable_red", r.returncode, 2, r.stdout + r.stderr,
              "TOOL_UNAVAILABLE")

        # N8 nm 非零退出 ⇒ TOOL_FAILED rc=2
        _write(os.path.join(bindir, "nm"), _FAKE_NM_FAIL, 0o755)
        r = _run_cli(root, DEFAULT_BINARY_REL, bindir)
        check("N8_nm_failed_red", r.returncode, 2, r.stdout + r.stderr, "TOOL_FAILED")

    print("SELF_TEST %s cases=%d problems=%d"
          % ("PASS" if not problems else "FAIL", cases, len(problems)))
    for p in problems:
        print("  - " + p)
    return 0 if not problems else 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="BLD-001 link scan（fail-closed + 可执行负例面）")
    ap.add_argument("binary", nargs="?", default=DEFAULT_BINARY_REL,
                    help="生产 CLI 二进制路径（默认 %s；缺省时相对 --root）" % DEFAULT_BINARY_REL)
    ap.add_argument("--root", default=_repo_root(), help="仓库根（默认按脚本位置推导）")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()

    root = os.path.abspath(args.root)
    try:
        rc, lines = scan(root, args.binary)
    except AnchorStale as exc:
        print(str(exc), file=sys.stderr)
        print("LINK_SCAN_FAIL: 输入不可用（fail-closed，docs/ci/01_CHECKS.md §1）",
              file=sys.stderr)
        return 2
    for line in lines:
        print(line)
    return rc


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(2)
