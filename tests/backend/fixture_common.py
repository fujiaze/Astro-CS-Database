#!/usr/bin/env python3
"""共享 phase2 fixture 制备（V8-CI-012 批次 3 引入）。

历史形态：p2006/p3003/p3004/p3005/p3006 统一引用 `run/temp/p2003_dbg/f1f2/
F1.hips`（V6.1 时代手工操作产物，run/ 属临时操作目录，见 AGENTS.md），该路径
从无 CI 注册的生成方 → hosted UT-BACKEND / DEEP-COV-PY 面相关族 setUpClass
全数 FileNotFoundError。

本模块提供 `ensure_f1f2_hips()`：按 test_p2001 同源逻辑编译 phase2 fixture
（tests/backend/phase2_fixture_main.cpp + AIO 链）并运行 `--make` 产出
F1.hips/F2.hips 到目标目录（默认维持历史路径 run/temp/p2003_dbg/f1f2，保证
既有 config 引用兼容）。进程内模块级缓存：fixture exe 只编译一次。
"""
import os
import re
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AIO = os.path.join(REPO, "lib", "astro_image_io")
FIXTURE_SRC = os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp")
DEFAULT_FDIR = os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2")

_FIXTURE_EXE = os.path.join(REPO, "run", "temp", "p2003_dbg", "fixture_exe")
_BUILT = False


def _cfitsio_objs(tmp):
    objs = []
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c"):
            continue
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
            continue
        o = os.path.join(tmp, f[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f),
                        "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    return objs


def _build_fixture_exe():
    """编译 phase2 fixture exe（与 test_p2001 setUpClass 同参数面）。"""
    global _BUILT
    if _BUILT and os.path.isfile(_FIXTURE_EXE):
        return _FIXTURE_EXE
    os.makedirs(os.path.dirname(_FIXTURE_EXE), exist_ok=True)
    incs = [f"-I{os.path.join(REPO, 'include')}",
            f"-I{os.path.join(AIO, 'include')}",
            f"-I{os.path.join(AIO, 'src')}",
            f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
            f"-I{os.path.join(REPO, 'lib', 'common')}",
            f"-I{os.path.join(REPO, 'lib', 'common', 'healpix')}"]
    srcs = [FIXTURE_SRC,
            os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
            os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
            os.path.join(AIO, "src", "aio_fits.cpp"),
            os.path.join(AIO, "src", "aio_api.cpp"),
            os.path.join(AIO, "src", "aio_log.cpp"),
            os.path.join(AIO, "src", "aio_compressor.cpp"),
            os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp")]
    r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                        *srcs, *_cfitsio_objs(os.path.dirname(_FIXTURE_EXE)),
                        "-lz", "-lzstd", "-llz4", "-o", _FIXTURE_EXE],
                       capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, "[fixture_common compile] " + r.stderr[-600:]
    _BUILT = True
    return _FIXTURE_EXE


def ensure_f1f2_hips(fdir=DEFAULT_FDIR):
    """确保 fdir 下存在 F1.hips/F2.hips（缺失时编译 fixture 并生成）。"""
    f1 = os.path.join(fdir, "F1.hips")
    f2 = os.path.join(fdir, "F2.hips")
    if os.path.isdir(f1) and os.path.isdir(f2):
        return fdir
    exe = _build_fixture_exe()
    os.makedirs(fdir, exist_ok=True)
    r = subprocess.run([exe, "--make", fdir], capture_output=True, text=True, timeout=300)
    assert "HIPS_FIXTURES_OK" in r.stdout, "[fixture_common make] " + r.stderr[-600:]
    assert os.path.isdir(f1) and os.path.isdir(f2), "F1/F2.hips 生成失败: " + fdir
    return fdir


SEAM6_DIR = os.path.join(REPO, "run", "temp", "p2007_seam6", "seam6")


def ensure_seam6_hips(fdir=SEAM6_DIR):
    """确保 fdir 下存在 P2-007 seam6 fixture: SEAM0..SEAM5.hips 共 6 块。

    fixture exe 的 `--make-seam6` 模式（P2-007 G5）按模式轮换/交替偏移生成 6 块
    mini HiPS，保证 UPM 可求解且 run ≥10s。历史形态该目录由手工操作产出
    （run/ 属临时操作目录），无 CI 注册生成方；本函数补齐同 ensure_f1f2_hips
    的自愈语义。
    """
    paths = [os.path.join(fdir, "SEAM%d.hips" % i) for i in range(6)]
    if all(os.path.isdir(p) for p in paths):
        return fdir
    exe = _build_fixture_exe()
    os.makedirs(fdir, exist_ok=True)
    r = subprocess.run([exe, "--make-seam6", fdir], capture_output=True, text=True,
                       timeout=300)
    assert "HIPS_FIXTURES_OK" in r.stdout, "[fixture_common make-seam6] " + r.stderr[-600:]
    assert all(os.path.isdir(p) for p in paths), "SEAM0..5.hips 生成失败: " + fdir
    return fdir


def two_cpu_preexec():
    """subprocess preexec_fn: 把子进程限制到前 2 个可用 CPU（恢复 2c 生产语境）。

    用途(实测语境适配, 非语义放宽): phase3 大图类测试(p1004/p3006)按原始 CI
    2c2g 设计语境运行 — 2c 下 gate 阈值 = 0.80*2 = 1.6 核, 大图 workload 可达;
    16c 全核下阈值 12.8 核超出 mini fixture 算力。budget 注入链 P0 修复后
    (module_adapters execute 以 ctx.budget() 权威注入) session budget = 真机
    分配核数(2c 语境下=2), 不再有 budget 恒 2 与 available 不一致问题; 此
    helper 仅提供确定性的小核数运行语境。
    仅 Linux（sched_setaffinity）；fork 后 exec 前执行，不得在此 import。
    """
    cpus = sorted(os.sched_getaffinity(0))
    if len(cpus) >= 2:
        os.sched_setaffinity(0, set(cpus[:2]))
