#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UT-CLI 共享 phase2 fixture 制备（CTESTFULL-01 修复）。

## 为什么需要

eng/tests/cli/ 下多个测试文件各自在 setUpClass 里重新编译**同一份** phase2
fixture（eng/tests/backend/phase2_fixture_main.cpp + vendored AIO 链 + vendored
cfitsio 全量 .c）。UT-CLI 因此在 linux-main 档里异常慢（现场 177 例用掉 985 s，
见 run/ci/run-checks/20260922T195709Z/checks/UT-CLI.json）：慢的不是断言，是
**同一份 fixture 被重复编译十几次**（每次都要把 cfitsio 约 50 个 .c 逐个
gcc -O2，再 g++ -O2 链接 8 个 TU）。

更要紧的是失败语义分裂。同一个 setUpClass 配方有两种写法：

* **fail-closed 面**（test_phase2_inprocess / test_phase3_inprocess /
  test_phase123_pipeline）：assert rc == 0, stderr[-N:]、
  assert "HIPS_FIXTURES_OK" in out, stderr —— 失败即带原因报错；
* **静默面**（test_monitor_events / test_iso_acr_gpu_isolation）：
  if rc == 0: ... if "HIPS_FIXTURES_OK" in out: cls.hips = ... ——
  编译或生成失败时 cls.hips 保持 None，测试只报
  「无合成 fixture（setUpClass 未产出 FIELD.hips）」，**真实原因（rc / stderr）
  被整段丢弃**，现场无法归因（这正是 CTESTFULL-01 的 B 面现象）。

本模块把两件事统一：**进程级缓存**（一次编译，整个 suite 复用，与
eng/tests/backend/fixture_common.py 同款语义）+ **fail-closed 且带原因**
（失败必须带 rc、命令与 stderr 尾部）。配方与 eng/ci/prepare_linux_fixtures.py
同源（同一份 vendored AIO 源、同一个 --make-field 入口）。

## 语义边界

* 只写 run/ 下（AGENTS.md：run/ 为 gitignore 临时区），不碰仓库跟踪面；
* 不放宽任何断言：本模块只负责「fixture 到底有没有造出来、没造出来的原因」，
  测试判据仍由各测试自己持有；
* 不新增 SKIP：工具缺失（g++）同样按 fail-closed 抛错，避免产生未登记的 SKIP。
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
AIO = os.path.join(REPO, "lib", "infrastructure", "aio")
SHARED = os.path.join(REPO, "lib", "algorithms", "shared")
CFITSIO_DIR = os.path.join(AIO, "third_party", "cfitsio")
FIXTURE_MAIN = os.path.join(REPO, "eng", "tests", "backend", "phase2_fixture_main.cpp")

# 落点：run/ 下（gitignore），固定路径便于跨测试文件复用与人工复核。
CACHE_DIR = os.path.join(REPO, "run", "temp", "ut_cli_fixture")
FIXTURE_EXE = os.path.join(CACHE_DIR, "fixture")

# 与 eng/ci/prepare_linux_fixtures.py / fixture_common.py 同款的 cfitsio 排除表。
_SKIP_FITS = re.compile(
    r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
    r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
    r"imcopy|imarith|tabcompile|sortcol|tabselect")

# 超时：旧实现 g++ 链接用 600 s、--make-field 用 300 s。linux-main 档并行道下
# 本机实测编译被并发检查拖慢数倍（见 run/CTESTFULL-01/），链接步放宽到 900 s
# 以覆盖受控并发；仍然显式设限（不无超时执行）。
COMPILE_TIMEOUT_S = 900
MAKE_FIELD_TIMEOUT_S = 300
CFITSIO_FILE_TIMEOUT_S = 300

_EXE = None  # 进程级缓存：一次编译，suite 内复用


class CliFixtureError(RuntimeError):
    """fixture 制备失败（fail-closed，消息必须含 rc 与 stderr 尾部）。"""


def _fixture_srcs():
    """fixture 直接 g++ 链接所需的全部 TU。

    CTESTFULL-01 根因：lib/infrastructure/aio/src/aio_file_io.h 的 inline
    aio_file::sha256_hex 调用 astrocs::crypto::Sha256（定义在
    lib/algorithms/shared/crypto/sha256.h）。生产构建由 astrocs_common 提供该
    TU（根 CMakeLists.txt:379）；fixture 不走 CMake，直接 g++ 时必须显式带上，
    否则链接期 "undefined reference to astrocs::crypto::Sha256::update(...)"。
    """
    return [
        os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
        os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
        os.path.join(AIO, "src", "aio_fits.cpp"),
        os.path.join(AIO, "src", "aio_api.cpp"),
        os.path.join(AIO, "src", "aio_log.cpp"),
        os.path.join(AIO, "src", "aio_compressor.cpp"),
        os.path.join(SHARED, "healpix", "healpix_core.cpp"),
        os.path.join(SHARED, "crypto", "sha256.cpp"),
    ]


def _incs():
    return [
        f"-I{os.path.join(REPO, 'lib', 'include')}",
        f"-I{os.path.join(AIO, 'include')}",
        f"-I{os.path.join(AIO, 'src')}",
        f"-I{CFITSIO_DIR}",
        f"-I{SHARED}",
        f"-I{os.path.join(SHARED, 'healpix')}",
    ]


def _cfitsio_objs(objdir: str) -> list:
    """vendored cfitsio 全量 .c -> .o（逐个编译，失败即带文件名与 stderr 抛错）。"""
    objs = []
    for f in sorted(os.listdir(CFITSIO_DIR)):
        if not f.endswith(".c") or _SKIP_FITS.search(f):
            continue
        o = os.path.join(objdir, f[:-2] + ".o")
        try:
            r = subprocess.run(["gcc", "-O2", "-w", f"-I{CFITSIO_DIR}",
                                "-c", os.path.join(CFITSIO_DIR, f), "-o", o],
                               capture_output=True, text=True,
                               timeout=CFITSIO_FILE_TIMEOUT_S)
        except subprocess.TimeoutExpired as exc:
            raise CliFixtureError(
                "[ut_cli_fixture] cfitsio %s 编译超时（>%ds）"
                % (f, CFITSIO_FILE_TIMEOUT_S)) from exc
        if r.returncode != 0:
            raise CliFixtureError(
                "[ut_cli_fixture] cfitsio %s 编译失败 rc=%d\n%s"
                % (f, r.returncode, (r.stderr or "")[-600:]))
        objs.append(o)
    if not objs:
        raise CliFixtureError("[ut_cli_fixture] cfitsio 源目录为空：%s" % CFITSIO_DIR)
    return objs


def fixture_exe() -> str:
    """返回 phase2 fixture 可执行文件路径（进程内只编译一次）。

    失败一律抛 CliFixtureError，消息含 rc / 超时 / stderr 尾部 —— 不再把
    失败折叠成 cls.hips = None 的「无合成 fixture」。
    """
    global _EXE
    if _EXE and os.path.isfile(_EXE):
        return _EXE
    if not shutil.which("g++"):
        raise CliFixtureError("[ut_cli_fixture] 宿主无 g++，无法编译 fixture（fail-closed）")
    if not os.path.isfile(FIXTURE_MAIN):
        raise CliFixtureError("[ut_cli_fixture] fixture 入口缺失：%s" % FIXTURE_MAIN)
    os.makedirs(CACHE_DIR, exist_ok=True)
    argv = ["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *_incs(),
            FIXTURE_MAIN, *_fixture_srcs(), *_cfitsio_objs(CACHE_DIR),
            "-lz", "-lzstd", "-llz4", "-o", FIXTURE_EXE]
    try:
        r = subprocess.run(argv, capture_output=True, text=True,
                           timeout=COMPILE_TIMEOUT_S)
    except subprocess.TimeoutExpired as exc:
        raise CliFixtureError(
            "[ut_cli_fixture] fixture 链接编译超时（>%ds）：%s"
            % (COMPILE_TIMEOUT_S, " ".join(argv[:6]))) from exc
    if r.returncode != 0:
        raise CliFixtureError(
            "[ut_cli_fixture] fixture 链接编译失败 rc=%d\n%s"
            % (r.returncode, (r.stderr or "")[-800:]))
    _EXE = FIXTURE_EXE
    return _EXE


def make_field_hips(dest_dir: str, *, cwd: str | None = None) -> str:
    """用 fixture 在 dest_dir 生成 FIELD.hips，返回其路径；失败带原因抛错。

    cwd 由调用方给出（UT-CLI 统一落 run/，见 cli_test_hygiene.run_cwd）。
    """
    exe = fixture_exe()
    os.makedirs(dest_dir, exist_ok=True)
    try:
        r = subprocess.run([exe, "--make-field", dest_dir], capture_output=True,
                           text=True, timeout=MAKE_FIELD_TIMEOUT_S, cwd=cwd)
    except subprocess.TimeoutExpired as exc:
        raise CliFixtureError(
            "[ut_cli_fixture] --make-field 超时（>%ds）" % MAKE_FIELD_TIMEOUT_S) from exc
    hips = os.path.join(dest_dir, "FIELD.hips")
    if "HIPS_FIXTURES_OK" not in (r.stdout or ""):
        raise CliFixtureError(
            "[ut_cli_fixture] --make-field 未产出 FIELD.hips：rc=%d\n"
            "stdout tail: %s\nstderr tail: %s"
            % (r.returncode, (r.stdout or "")[-400:], (r.stderr or "")[-400:]))
    if not os.path.isdir(hips):
        raise CliFixtureError("[ut_cli_fixture] 标记 OK 但 FIELD.hips 不存在：%s" % hips)
    return hips
