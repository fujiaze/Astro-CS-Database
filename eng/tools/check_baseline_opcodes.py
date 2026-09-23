#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""baseline opcode scanner (05 §2/05 §7) — ABI-003 验收门 1。

反汇编 baseline_backend.o, 禁止超出最低 amd64(SSE2)合同的 SIMD 指令
（VEX 前缀助记符 v* 与 %ymm/%zmm 寄存器）。

对象路径是**构建布局相关**的（ARCH-001 迁移前后分别落在
<build>/CMakeFiles/astrocs_cpu_baseline.dir/lib/backend_host/ 与
.../lib/infrastructure/benchmark/backend_host/），因此本门不写死单一相对路径：
  * 显式位置参数 <obj> 优先；
  * 否则在 --build-dir（默认 build，与 deep_ci_driver --build-dir 同口径）下按
    BASELINE_OBJ_GLOB 解析；解析到多个候选则**逐个全查**（任一含 AVX 即判红），
    解析到零个 ⇒ fail-closed 判红。

fail-closed（docs/ci/01_CHECKS.md §1:12–15）
  * 锚存活：--build-dir 不存在 / 对象解析为空 / 显式对象不存在 ⇒
    ANCHOR_STALE: <常量名> <路径> 具名失败（rc=2），不 traceback、不静默判绿；
  * objdump 不可用 / 非零退出 / 反汇编输出零指令 ⇒ 判红（rc=2）
    （§1:13「scanned == 0 ⇒ rc != 0」）。

可执行负例面（§1:10–11）
  --self-test：临时沙箱内用假 objdump（PATH 注入）真跑本 CLI 子进程，逐例断言 rc 与
  具名结论：SSE2 正例绿 / ymm 寄存器红 / VEX 助记符红 / 空反汇编红 / objdump 缺失红 /
  objdump 非零退出红 / 构建目录无对象 ANCHOR_STALE 红 / 显式对象缺失 ANCHOR_STALE 红 /
  --build-dir 解析到对象绿。

用法
  python3 eng/tools/check_baseline_opcodes.py [obj] [--build-dir DIR] [--self-test]
exit 0 = PASS；1 = 判据违规；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys

# ── 判据锚（§1 锚存活） ────────────────────────────────────────────────────
BUILD_DIR_REL = "build"
OBJ_BASENAME = "baseline_backend.cpp.o"
BASELINE_TARGET_DIR = "astrocs_cpu_baseline.dir"
OBJDUMP_TOOL = "objdump"

AVX_MNEMONICS = re.compile(
    r"\b(v(?:add|sub|mul|div|mov|fm|broadcast|blend|pxor|ptest|gather|extract|insert|"
    r"perm|shuffle|round|sqrt|rcp|rsqrt|hadd|hsub|unpck|punpck|cvtd|cvtp|cvt|and|or|xor)"
    r"[a-z0-9]*)\b")
YMM_ZMM = re.compile(r"%[yz]mm\d*")
# objdump -d 的指令行形态：<前导空白><hex 偏移>":"（符号标题行是 "<hex> <sym>:"，不匹配）
INSN_RE = re.compile(r"^\s*[0-9a-f]+:")


class AnchorStale(Exception):
    """锚失效 / 扫描面塌缩 —— fail-closed，rc=2（§1 锚存活 + fail-closed）。"""


def _repo_root() -> str:
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def find_objects(build_dir: str) -> list[str]:
    """在 <build_dir>/CMakeFiles/astrocs_cpu_baseline.dir/** 下解析 baseline 对象。

    零候选 ⇒ AnchorStale（具名点名，不静默判绿）。
    """
    if not os.path.isdir(build_dir):
        raise AnchorStale("ANCHOR_STALE: BUILD_DIR %s" % build_dir)
    base = os.path.join(build_dir, "CMakeFiles", BASELINE_TARGET_DIR)
    found: list[str] = []
    if os.path.isdir(base):
        for dirpath, _dirnames, filenames in os.walk(base):
            for fn in filenames:
                if fn == OBJ_BASENAME:
                    found.append(os.path.join(dirpath, fn))
    if not found:
        raise AnchorStale("ANCHOR_STALE: BASELINE_OBJ_GLOB %s"
                          % os.path.join(build_dir, "CMakeFiles", BASELINE_TARGET_DIR,
                                         "**", OBJ_BASENAME).replace(os.sep, "/"))
    return sorted(found)


def disassemble(obj: str) -> str:
    exe = shutil.which(OBJDUMP_TOOL)
    if exe is None:
        raise AnchorStale("TOOL_UNAVAILABLE: %s 不在 PATH 上（反汇编面不可用）" % OBJDUMP_TOOL)
    proc = subprocess.run([exe, "-d", obj], capture_output=True, text=True, timeout=300)
    if proc.returncode != 0:
        raise AnchorStale("TOOL_FAILED: %s -d %s rc=%d %s"
                          % (OBJDUMP_TOOL, obj, proc.returncode, proc.stderr.strip()[:160]))
    return proc.stdout


def scan_disassembly(text: str) -> tuple[list[tuple[str, str]], int]:
    """返回 (违规项 [(原因, 行)], 指令行数)。"""
    bad: list[tuple[str, str]] = []
    n_ins = 0
    for line in text.splitlines():
        if INSN_RE.match(line):
            n_ins += 1
        if YMM_ZMM.search(line):
            bad.append(("ymm/zmm register", line.strip()))
            continue
        m = AVX_MNEMONICS.search(line)
        if m:
            # v-前缀助记符均为 VEX/AVX 家族; SSE2 无 v-前缀
            bad.append(("AVX mnemonic %s" % m.group(1), line.strip()))
    return bad, n_ins


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="baseline opcode scanner（ABI-003 验收门 1；--build-dir 解析对象）")
    ap.add_argument("obj", nargs="?", default=None,
                    help="显式对象文件（缺省时由 --build-dir 解析）")
    ap.add_argument("--build-dir", default=BUILD_DIR_REL,
                    help="构建目录（默认 %s；对象按 %s/**/%s 解析）"
                         % (BUILD_DIR_REL, BASELINE_TARGET_DIR, OBJ_BASENAME))
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()

    try:
        if args.obj:
            objs = [args.obj]
            if not os.path.isfile(args.obj):
                raise AnchorStale("ANCHOR_STALE: OBJ_ARG %s" % args.obj)
        else:
            build_dir = args.build_dir if os.path.isabs(args.build_dir) \
                else os.path.join(_repo_root(), args.build_dir)
            objs = find_objects(build_dir)
        total_bad: list[tuple[str, str, str]] = []
        for obj in objs:
            text = disassemble(obj)
            bad, n_ins = scan_disassembly(text)
            if n_ins == 0:
                raise AnchorStale("DISASM_EMPTY: %s 反汇编输出零指令 —— "
                                  "扫描面塌缩，拒绝判绿" % obj)
            if bad:
                total_bad.extend((obj, why, ln) for why, ln in bad)
            else:
                print("BASELINE_OPCODE_PASS %s instructions=%d no-VEX/ymm/zmm"
                      % (obj, n_ins))
    except AnchorStale as exc:
        print(str(exc), file=sys.stderr)
        print("BASELINE_OPCODE_FAIL: 输入不可用（fail-closed，docs/ci/01_CHECKS.md §1）",
              file=sys.stderr)
        return 2
    if total_bad:
        print("BASELINE_OPCODE_FAIL (%d)" % len(total_bad))
        for obj, why, ln in total_bad[:20]:
            print("  %s: %s: %s" % (obj, why, ln[:100]))
        return 1
    return 0


# ────────────────────────────────────────────────────────────── self-test
# 假工具用**绝对解释器**作 shebang：self-test 会把 PATH 收窄到只含假工具目录，
# 因此假工具自身不得依赖 PATH 上的任何外部命令（cat/echo 之类会 ENOENT）。
def _fake_tool(py_body: str) -> str:
    return "#!%s\n%s" % (sys.executable, py_body)


def _fake_stdout(lines: list[str]) -> str:
    """假 objdump：把给定行原样写到 stdout 后 exit 0。"""
    return _fake_tool("import sys\nsys.stdout.write(%r)\n" % ("\n".join(lines) + "\n"))


_SSE2_LINES = [
    "0000000000000000 <backend_warmup>:",
    "       0:\t48 85 ff             \ttest   %rdi,%rdi",
    "       3:\t74 13                \tje     18 <backend_warmup+0x18>",
    "       5:\t31 c0                \txor    %eax,%eax",
    "       7:\t0f 95 c0             \tsetne  %al",
    "       a:\t66 0f ef c0          \tpxor   %xmm0,%xmm0",
    "       e:\tf2 0f 10 07          \tmovsd  (%rdi),%xmm0",
]
_YMM_LINES = [
    "0000000000000000 <backend_warmup>:",
    "       0:\t48 85 ff             \ttest   %rdi,%rdi",
    "       3:\tc5 fd 6f 07          \tvmovdqu (%rdi),%ymm0",
]
_VEX_LINES = [
    "0000000000000000 <backend_warmup>:",
    "       0:\t48 85 ff             \ttest   %rdi,%rdi",
    "       3:\tc5 f8 58 c1          \tvaddps %xmm1,%xmm0,%xmm0",
]
_FAKE_OBJDUMP_SSE2 = _fake_stdout(_SSE2_LINES)
_FAKE_OBJDUMP_YMM = _fake_stdout(_YMM_LINES)
_FAKE_OBJDUMP_VEX = _fake_stdout(_VEX_LINES)
_FAKE_OBJDUMP_EMPTY = _fake_tool("import sys\nsys.exit(0)\n")
_FAKE_OBJDUMP_FAIL = _fake_tool(
    "import sys\nsys.stderr.write(\"objdump: 'x': No such file\\n\")\nsys.exit(1)\n")


def _write(path: str, text: str, mode: int | None = None) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)
    if mode is not None:
        os.chmod(path, mode)


def _run_cli(args: list[str], path_dir: str | None) -> subprocess.CompletedProcess:
    env = dict(os.environ)
    if path_dir is not None:
        env["PATH"] = path_dir
    return subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                          capture_output=True, text=True, timeout=300, env=env)


def self_test() -> int:
    """可执行正/负例面（§1:10–11 / ENGINEERING_SPEC §8）。"""
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

    with tempfile.TemporaryDirectory(prefix="baseline_op_st_") as td:
        root = os.path.join(td, "repo")
        bindir = os.path.join(td, "bin")
        empty_bindir = os.path.join(td, "emptybin")
        os.makedirs(empty_bindir)
        obj = os.path.join(root, "build", "CMakeFiles", BASELINE_TARGET_DIR,
                           "lib", "backend_host", OBJ_BASENAME)
        _write(obj, "ELF-fake\n")

        # P1 正例：SSE2 指令 ⇒ rc=0 且指令计数非 0（旧实现的 l.strip()[:2] 统计恒 0）
        _write(os.path.join(bindir, OBJDUMP_TOOL), _FAKE_OBJDUMP_SSE2, 0o755)
        r = _run_cli([obj], bindir)
        check("P1_sse2_green", r.returncode, 0, r.stdout + r.stderr,
              "instructions=6 no-VEX/ymm/zmm")

        # P2 --build-dir 解析到对象 ⇒ rc=0
        r = _run_cli(["--build-dir", os.path.join(root, "build")], bindir)
        check("P2_build_dir_resolve_green", r.returncode, 0, r.stdout + r.stderr,
              "BASELINE_OPCODE_PASS")

        # N1 ymm 寄存器 ⇒ rc=1
        _write(os.path.join(bindir, OBJDUMP_TOOL), _FAKE_OBJDUMP_YMM, 0o755)
        r = _run_cli([obj], bindir)
        check("N1_ymm_register_red", r.returncode, 1, r.stdout + r.stderr,
              "ymm/zmm register")

        # N2 VEX 助记符 ⇒ rc=1
        _write(os.path.join(bindir, OBJDUMP_TOOL), _FAKE_OBJDUMP_VEX, 0o755)
        r = _run_cli([obj], bindir)
        check("N2_vex_mnemonic_red", r.returncode, 1, r.stdout + r.stderr,
              "AVX mnemonic vaddps")

        # N3 空反汇编 ⇒ 扫描面塌缩 rc=2
        _write(os.path.join(bindir, OBJDUMP_TOOL), _FAKE_OBJDUMP_EMPTY, 0o755)
        r = _run_cli([obj], bindir)
        check("N3_disasm_empty_red", r.returncode, 2, r.stdout + r.stderr, "DISASM_EMPTY")

        # N4 objdump 不可用 ⇒ rc=2
        r = _run_cli([obj], empty_bindir)
        check("N4_objdump_unavailable_red", r.returncode, 2, r.stdout + r.stderr,
              "TOOL_UNAVAILABLE")

        # N5 objdump 非零退出 ⇒ rc=2
        _write(os.path.join(bindir, OBJDUMP_TOOL), _FAKE_OBJDUMP_FAIL, 0o755)
        r = _run_cli([obj], bindir)
        check("N5_objdump_failed_red", r.returncode, 2, r.stdout + r.stderr, "TOOL_FAILED")

        # N6 构建目录下无 baseline 对象 ⇒ ANCHOR_STALE rc=2
        _write(os.path.join(bindir, OBJDUMP_TOOL), _FAKE_OBJDUMP_SSE2, 0o755)
        empty_build = os.path.join(root, "build_empty")
        os.makedirs(empty_build)
        r = _run_cli(["--build-dir", empty_build], bindir)
        check("N6_no_object_anchor_stale", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: BASELINE_OBJ_GLOB")

        # N7 --build-dir 不存在 ⇒ ANCHOR_STALE rc=2
        r = _run_cli(["--build-dir", os.path.join(root, "no_such_build")], bindir)
        check("N7_build_dir_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: BUILD_DIR")

        # N8 显式对象不存在 ⇒ ANCHOR_STALE rc=2
        r = _run_cli([os.path.join(root, "no_such.o")], bindir)
        check("N8_obj_arg_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: OBJ_ARG")

    print("SELF_TEST %s cases=%d problems=%d"
          % ("PASS" if not problems else "FAIL", cases, len(problems)))
    for p in problems:
        print("  - " + p)
    return 0 if not problems else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(2)
