#!/usr/bin/env python3
"""共享 phase2 fixture 制备（V8-CI-012 批次 3 引入）。

历史形态：p2006/p3003/p3004/p3005/p3006 统一引用 `run/temp/p2003_dbg/f1f2/
F1.hips`（V6.1 时代手工操作产物，run/ 属临时操作目录，见 AGENTS.md），该路径
从无 CI 注册的生成方 → hosted UT-BACKEND / DEEP-COV-PY 面相关族 setUpClass
全数 FileNotFoundError。

本模块提供 `ensure_f1f2_hips()`：按 test_p2001 同源逻辑编译 phase2 fixture
（eng/tests/backend/phase2_fixture_main.cpp + AIO 链）并运行 `--make` 产出
F1.hips/F2.hips 到目标目录（默认维持历史路径 run/temp/p2003_dbg/f1f2，保证
既有 config 引用兼容）。进程内模块级缓存：fixture exe 只编译一次。
"""
import os
import re
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
# CLI-002/ROOT-008 重锚: AIO 落 lib/infrastructure/aio(旧 lib/astro_image_io 已退役);
# 共享算法基础库落 lib/algorithms/shared(旧 lib/common 已退役)。
AIO = os.path.join(REPO, "lib", "infrastructure", "aio")
SHARED = os.path.join(REPO, "lib", "algorithms", "shared")
FIXTURE_SRC = os.path.join(REPO, "eng", "tests", "backend", "phase2_fixture_main.cpp")
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
    incs = [f"-I{os.path.join(REPO, 'lib', 'include')}",
            f"-I{os.path.join(AIO, 'include')}",
            f"-I{os.path.join(AIO, 'src')}",
            f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
            f"-I{SHARED}",
            f"-I{os.path.join(SHARED, 'healpix')}"]
    srcs = [FIXTURE_SRC,
            os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
            os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
            os.path.join(AIO, "src", "aio_fits.cpp"),
            os.path.join(AIO, "src", "aio_api.cpp"),
            os.path.join(AIO, "src", "aio_log.cpp"),
            os.path.join(AIO, "src", "aio_compressor.cpp"),
            os.path.join(SHARED, "healpix", "healpix_core.cpp")]
    r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                        *srcs, *_cfitsio_objs(os.path.dirname(_FIXTURE_EXE)),
                        "-lz", "-lzstd", "-llz4", "-o", _FIXTURE_EXE],
                       capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, "[fixture_common compile] " + r.stderr[-600:]
    _BUILT = True
    return _FIXTURE_EXE


# FIX-402: Phase3 生产输入语义守卫（ASTROCS_DESIGN §6.3 / FZ-BUNIT-SEMANTICS）
# 只接受**显式声明**面亮度语义的输入。fixture 由 AIO writer 生成（writer 不写
# BUNIT），故此处按冻结单位表补齐产品单位声明（与 module_adapters 的
# declare_hips_surface_brightness_units 同源同串; 幂等）。
#   signal   : BUNIT=ADU/sr   (signal_sb, pixel_area_power=-2)
#   variance : BUNIT=ADU^2/sr^2 (sb_variance_out, -4; FZ-P3-BUNIT-QUADRATIC)
#   ivar     : BUNIT=sr^2/ADU^2 (sb_ivar_out, +4)
_UNIT_DECL = {
    "signal": ("ADU/sr", -2),
    "variance": ("ADU^2/sr^2", -4),
    "ivar": ("sr^2/ADU^2", 4),
}


def ensure_hips_unit_declaration(hips_dir):
    """幂等补齐 HiPS 子产品单位/像素语义声明（FIX-402 fixture 迁移）。"""
    for sub, (bunit, power) in _UNIT_DECL.items():
        path = os.path.join(hips_dir, sub, "properties")
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8") as f:
            lines = f.read().splitlines()
        keep = []
        for ln in lines:
            key = ln.split("=", 1)[0].strip()
            if key in ("BUNIT", "bunit", "ASTROCS_SIGNAL_UNIT",
                       "ASTROCS_PIXEL_SEMANTICS", "ASTROCS_PIXEL_AREA_POWER"):
                continue
            keep.append(ln)
        keep += ["BUNIT=%s" % bunit,
                 "ASTROCS_SIGNAL_UNIT=ADU/sr",
                 "ASTROCS_PIXEL_SEMANTICS=surface_brightness",
                 "ASTROCS_PIXEL_AREA_POWER=%d" % power]
        tmp = path + ".fix402.tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            f.write("\n".join(keep) + "\n")
        os.replace(tmp, path)
    return hips_dir


def ensure_f1f2_hips(fdir=DEFAULT_FDIR):
    """确保 fdir 下存在 F1.hips/F2.hips（缺失时编译 fixture 并生成）。"""
    f1 = os.path.join(fdir, "F1.hips")
    f2 = os.path.join(fdir, "F2.hips")
    if os.path.isdir(f1) and os.path.isdir(f2):
        # FIX-402: 既有缓存 fixture（writer 未写 BUNIT）同样补齐声明, 幂等。
        ensure_hips_unit_declaration(f1)
        ensure_hips_unit_declaration(f2)
        return fdir
    exe = _build_fixture_exe()
    os.makedirs(fdir, exist_ok=True)
    r = subprocess.run([exe, "--make", fdir], capture_output=True, text=True, timeout=300)
    assert "HIPS_FIXTURES_OK" in r.stdout, "[fixture_common make] " + r.stderr[-600:]
    assert os.path.isdir(f1) and os.path.isdir(f2), "F1/F2.hips 生成失败: " + fdir
    ensure_hips_unit_declaration(f1)
    ensure_hips_unit_declaration(f2)
    return fdir


SEAM6_DIR = os.path.join(REPO, "run", "temp", "p2007_seam6", "seam6")
SEAM_ROOT = os.path.join(REPO, "run", "temp", "p2007_seam")


def ensure_seam_hips(n, fdir=None):
    """确保存在 n 块 P2-007 seam fixture: SEAM0..SEAM{n-1}.hips；返回 (fdir, paths)。

    RESCUE-FD-08b(自标定 workload): p2007 用例先在 test 侧用小样本实测吞吐, 再据
    实测选块数, 使 active_wall 稳过 10s 冻结锚(不拍固定数字, 不受宿主速度漂移影响)。
    fixture exe 的 `--make-seam-n <dir> <N>` 生成 N 块模式轮换/偏移交替的 mini
    HiPS; 缺省落 `run/temp/p2007_seam/n<N>`(按 N 缓存)。
    """
    if fdir is None:
        fdir = os.path.join(SEAM_ROOT, "n%d" % n)
    paths = [os.path.join(fdir, "SEAM%d.hips" % i) for i in range(n)]
    if all(os.path.isdir(p) for p in paths):
        return fdir, paths
    exe = _build_fixture_exe()
    os.makedirs(fdir, exist_ok=True)
    r = subprocess.run([exe, "--make-seam-n", fdir, str(n)], capture_output=True,
                       text=True, timeout=900)
    assert "HIPS_FIXTURES_OK" in r.stdout, "[fixture_common make-seam-n] " + r.stderr[-600:]
    assert all(os.path.isdir(p) for p in paths), "SEAM0..N-1.hips 生成失败: " + fdir
    return fdir, paths


def ensure_seam6_hips(fdir=SEAM6_DIR):
    """确保 fdir 下存在 P2-007 seam6 fixture: SEAM0..SEAM5.hips 共 6 块（兼容入口）。"""
    fdir2, paths = ensure_seam_hips(6, fdir)
    assert all(os.path.isdir(p) for p in paths), "SEAM0..5.hips 生成失败: " + fdir2
    return fdir2


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
