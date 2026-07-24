# -*- coding: utf-8 -*-
"""
端到端编排器测试
功能: 测试 Orchestrator 类的端到端管线处理 (校准→解析→PSF→光度→drizzle)
用途: 验证各阶段调用顺序、块传递、错误处理是否正确，以及无临时文件的内存管线
依赖: 所有管线模块的 DLL 和数据库 (Gaia DR3SP)

本文件从 lib/astro_image_io/python/tests/test_orchestrator_e2e.py 迁移而来。
路径深度已更新（从 lib/orchestrator/tests/ 出发仅需 3 级回到项目根目录）。

运行:
    cd "f:\Astro dev\Astro CS Normalization Database\lib\orchestrator\tests"
    python test_orchestrator_e2e.py

注意:
    - 测试帧: testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_...-600S-Lum.fts
    - 跳过校准 (无 master_bias/dark/flat)
    - plate_solve 需要 Gaia 数据库 (GaiaDR3SP 目录)
    - photometric 无 f_syn 数据，走退化路径
    - 某些模块不可用 (DLL/数据库) 时测试会失败，这是正常的
"""

from __future__ import annotations

import glob
import os
import sys
import tempfile
import time
import traceback

# ============================================================================
# 路径设置 (必须在导入任何项目模块之前完成)
# ============================================================================

HERE = os.path.dirname(os.path.abspath(__file__))
# 新位置: lib/orchestrator/tests/ -> ../../.. = 项目根目录
PROJECT_ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
LIB_DIR = os.path.join(PROJECT_ROOT, "lib")

# MinGW DLL 路径 (C++ 模块依赖)
MINGW_BIN = r"C:\msys64\mingw64\bin"
if os.path.isdir(MINGW_BIN) and MINGW_BIN not in os.environ.get("PATH", ""):
    os.environ["PATH"] = MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(MINGW_BIN)
    except (OSError, FileNotFoundError):
        pass

# astro_image_io.dll 目录
ASTRO_IO_DIR = os.path.join(LIB_DIR, "astro_image_io")
if ASTRO_IO_DIR not in os.environ.get("PATH", ""):
    os.environ["PATH"] = ASTRO_IO_DIR + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(ASTRO_IO_DIR)
    except (OSError, FileNotFoundError):
        pass

# Python 模块路径
_MODULE_PATHS = [
    "orchestrator/python",            # 新增: 编排器本地路径
    "orchestrator/python/pipeline_adapters",  # 新增: 适配器目录
    "astro_image_io/python",
    "calibration/python",
    "plate_solve/python",
    "plate_solve/archive/vector_method/python/python",
    "photometric_calib/flux_calibrator/python",
    "healpix_db/healpix_drizzle",
    "dynamic_psf/python",
    "star_detector/python",
]
for _module_dir in _MODULE_PATHS:
    _p = os.path.join(LIB_DIR, _module_dir)
    if os.path.isdir(_p) and _p not in sys.path:
        sys.path.insert(0, _p)


# ============================================================================
# 参数类加载 (使用 importlib 避免多个 pipeline_adapter.py 命名冲突)
# ============================================================================

import importlib.util


def _load_module(module_name, file_path):
    """从文件路径加载 Python 模块"""
    if module_name in sys.modules:
        return sys.modules[module_name]
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = mod
    spec.loader.exec_module(mod)
    return mod


def _load_params():
    """加载各阶段参数类

    返回: (CalibrateParams, PlateSolveParams, PhotometricParams, DrizzleParams)
          加载失败的返回 None
    """
    CalibrateParams = None
    PlateSolveParams = None
    PhotometricParams = None
    DrizzleParams = None

    try:
        mod = _load_module(
            "_test_calib_adapter",
            os.path.join(LIB_DIR, "orchestrator", "python", "pipeline_adapters",
                         "calibrate_adapter.py"),
        )
        CalibrateParams = mod.CalibrateParams
        print("[OK] CalibrateParams 加载成功")
    except Exception as e:
        print(f"[FAIL] CalibrateParams 加载失败: {e}")

    try:
        mod = _load_module(
            "_test_solve_adapter",
            os.path.join(LIB_DIR, "orchestrator", "python", "pipeline_adapters",
                         "platesolve_adapter.py"),
        )
        PlateSolveParams = mod.PlateSolveParams
        print("[OK] PlateSolveParams 加载成功")
    except Exception as e:
        print(f"[FAIL] PlateSolveParams 加载失败: {e}")

    try:
        mod = _load_module(
            "_test_photo_adapter",
            os.path.join(LIB_DIR, "orchestrator", "python", "pipeline_adapters",
                         "photometric_adapter.py"),
        )
        PhotometricParams = mod.PhotometricParams
        print("[OK] PhotometricParams 加载成功")
    except Exception as e:
        print(f"[FAIL] PhotometricParams 加载失败: {e}")

    try:
        mod = _load_module(
            "_test_drizzle_adapter",
            os.path.join(LIB_DIR, "orchestrator", "python", "pipeline_adapters",
                         "drizzle_adapter.py"),
        )
        DrizzleParams = mod.DrizzleParams
        print("[OK] DrizzleParams 加载成功")
    except Exception as e:
        print(f"[FAIL] DrizzleParams 加载失败: {e}")

    return CalibrateParams, PlateSolveParams, PhotometricParams, DrizzleParams


# ============================================================================
# 测试主函数
# ============================================================================

def _list_temp_fits():
    """列出系统临时目录中的 .fits/.fts 文件 (用于检测临时文件泄漏)"""
    temp_dir = tempfile.gettempdir()
    files = set()
    for ext in ("*.fits", "*.fts", "*.fit"):
        for f in glob.glob(os.path.join(temp_dir, ext)):
            files.add(os.path.normpath(f))
    return files


def _validate_result(result, output_dir):
    """验证编排器返回的结果 dict

    参数:
        result: orchestrator.run_single 返回的 dict
        output_dir: 输出目录

    返回: (success, checks)
        success: bool - 所有关键验证是否通过
        checks: list[(name, passed, detail)] - 验证项列表
    """
    checks = []

    def check(name, passed, detail=""):
        checks.append((name, passed, detail))
        status = "[OK]" if passed else "[FAIL]"
        print(f"  {status} {name}: {detail}")
        return passed

    print("\n--- 验证结果 ---")

    # 1. 整体成功
    check("整体成功", result.get("success", False),
          f"success={result.get('success')}, error={result.get('error')}")

    # 2. 各阶段耗时
    timings = result.get("timings", {})
    print("\n  各阶段耗时:")
    total = 0.0
    for stage in ["read_fits", "calibrate", "platesolve", "psf_fit", "photometric", "drizzle"]:
        if stage in timings:
            t = timings[stage]
            total += t
            print(f"    {stage}: {t:.2f}s")
    print(f"    总计: {total:.2f}s")
    check("耗时记录非空", len(timings) > 0, f"记录了 {len(timings)} 个阶段")

    # 3. 块状态验证
    blocks = result.get("blocks", {})

    # 3a. PLATESOLVE 后应有 star_det, gaia_cat, header
    after_ps = blocks.get("after_platesolve", [])
    has_star_det = "star_det" in after_ps
    has_gaia_cat = "gaia_cat" in after_ps
    has_header = "header" in after_ps
    check("PLATESOLVE后 star_det 块存在", has_star_det,
          f"blocks={after_ps}")
    check("PLATESOLVE后 gaia_cat 块存在", has_gaia_cat,
          f"blocks={after_ps}")
    check("PLATESOLVE后 header 块存在", has_header,
          f"blocks={after_ps}")

    # 3b. PSF_FIT 后应有 psf 块
    after_psf = blocks.get("after_psf_fit", [])
    has_psf = "psf" in after_psf
    check("PSF_FIT后 psf 块存在", has_psf,
          f"blocks={after_psf}")

    # 3c. PHOTOMETRIC 后应有 data, photo_stats 块，不应有 grad_map
    after_photo = blocks.get("after_photometric", [])
    has_data = "data" in after_photo
    has_photo_stats = "photo_stats" in after_photo
    no_grad_map = "grad_map" not in after_photo
    check("PHOTOMETRIC后 data 块存在 (被校准后替换)", has_data,
          f"blocks={after_photo}")
    check("PHOTOMETRIC后 photo_stats 块存在", has_photo_stats,
          f"blocks={after_photo}")
    check("PHOTOMETRIC后 无 grad_map 块 (已简化)", no_grad_map,
          f"blocks={after_photo}")

    # 4. WCS 字段验证
    wcs = result.get("wcs", {})
    has_cd11 = "CD1_1" in wcs
    check("header 含 WCS 字段 CD1_1", has_cd11,
          f"wcs keys={list(wcs.keys())}")

    # 5. photo_stats KV 值验证
    photo_stats = result.get("photo_stats", {})
    has_n_matched = "N_MATCHED" in photo_stats
    has_scale = "SCALE_FACTOR" in photo_stats
    check("photo_stats 含 N_MATCHED", has_n_matched,
          f"photo_stats={photo_stats}")
    check("photo_stats 含 SCALE_FACTOR", has_scale,
          f"photo_stats={photo_stats}")

    # 6. .ahpx 输出文件验证
    output_files = result.get("output_files", [])
    check("输出文件列表非空", len(output_files) > 0,
          f"output_files={output_files}")

    ahpx_ok = False
    ahpx_size = 0
    if output_files:
        ahpx_path = output_files[0]
        if os.path.isfile(ahpx_path):
            ahpx_size = os.path.getsize(ahpx_path)
            ahpx_ok = ahpx_size > 0
            check(".ahpx 文件存在且非空", ahpx_ok,
                  f"path={ahpx_path}, size={ahpx_size} bytes")
            # 尝试读取 .ahpx header JSON 验证
            try:
                from astro_image_io import AhpxReader
                reader = AhpxReader(ahpx_path)
                w, h, c = reader.image_info
                hdr_json = reader.header_json
                reader.close()
                check(".ahpx header JSON 可读", True,
                      f"image=({w}x{h}x{c}), json_len={len(hdr_json)}")
            except Exception as e:
                check(".ahpx header JSON 可读", False,
                      f"读取失败: {e}")
        else:
            check(".ahpx 文件存在且非空", False,
                  f"文件不存在: {ahpx_path}")
    else:
        check(".ahpx 文件存在且非空", False, "无输出文件")

    # 汇总
    n_pass = sum(1 for _, p, _ in checks if p)
    n_total = len(checks)
    all_pass = n_pass == n_total
    print(f"\n  验证汇总: {n_pass}/{n_total} 通过")
    return all_pass, checks


def test_e2e():
    """端到端测试 - 校准→解析→PSF→光度→drizzle 全链路"""

    # 测试帧路径
    fits_path = os.path.join(
        PROJECT_ROOT, "testdata", "LDN43_T2素材_flying_dutchman", "lights",
        "LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts",
    )

    if not os.path.isfile(fits_path):
        print(f"[FAIL] 测试帧不存在: {fits_path}")
        return False

    print(f"测试帧: {fits_path}")
    print(f"文件大小: {os.path.getsize(fits_path) / 1024 / 1024:.1f} MB")

    # 输出目录
    output_dir = os.path.join(HERE, "output")
    log_dir = os.path.join(HERE, "logs")
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(log_dir, exist_ok=True)

    # 加载参数类
    print("\n--- 加载参数类 ---")
    CalibrateParams, PlateSolveParams, PhotometricParams, DrizzleParams = _load_params()

    # 构造参数
    print("\n--- 构造阶段参数 ---")

    # 校准: 跳过 (无 master_bias/dark/flat)
    calib_params = None
    print("[SKIP] calib_params=None (跳过校准，无 master 帧)")

    # 解析: 使用默认参数 (env=None 触发 init_environment)
    # 注意: plate_solve 使用 ipv_solve_from_memory 内存接口, 无临时 FITS 文件
    solve_params = None
    if PlateSolveParams is not None:
        try:
            solve_params = PlateSolveParams()  # env=None, gaia_mag_high=18.0
            print("[OK] solve_params=PlateSolveParams(env=None) [内存接口, 无临时文件]")
        except Exception as e:
            print(f"[FAIL] PlateSolveParams 构造失败: {e}")
    else:
        print("[SKIP] PlateSolveParams 不可用")

    # 光度校准: 使用默认参数 (无 f_syn，走退化路径)
    # 注意: photometric_calib 使用简化版 C++ DLL, 无 grad_map 块
    photo_params = None
    if PhotometricParams is not None:
        try:
            photo_params = PhotometricParams(log_dir=log_dir)
            print("[OK] photo_params=PhotometricParams(log_dir=...) [C++ DLL, 无grad_map]")
        except Exception as e:
            print(f"[FAIL] PhotometricParams 构造失败: {e}")
    else:
        print("[SKIP] PhotometricParams 不可用")

    # Drizzle: 指定输出目录
    # nside=8192 (分辨率1.57"/px, 优于源图像~1"/px的2倍, 满足奈奎斯特采样)
    # 注意: drizzle 使用 hp_drizzle_run 命名块直通, 无临时 FITS 文件
    drizzle_params = None
    if DrizzleParams is not None:
        try:
            drizzle_params = DrizzleParams(nside=8192, nested=True, pixfrac=0.8, output_dir=output_dir)
            print(f"[OK] drizzle_params=DrizzleParams(nside=8192, output_dir={output_dir}) [命名块直通, 无临时文件]")
        except Exception as e:
            print(f"[FAIL] DrizzleParams 构造失败: {e}")
    else:
        print("[SKIP] DrizzleParams 不可用")

    # 至少需要一个阶段
    if all(p is None for p in [calib_params, solve_params, photo_params, drizzle_params]):
        print("\n[FAIL] 所有阶段参数都为 None，无法测试")
        return False

    # 创建编排器
    print("\n--- 创建 Orchestrator ---")
    try:
        from orchestrator import Orchestrator
        orch = Orchestrator(
            calib_params=calib_params,
            solve_params=solve_params,
            photo_params=photo_params,
            drizzle_params=drizzle_params,
            log_dir=log_dir,
        )
        print("[OK] Orchestrator 创建成功")
    except Exception as e:
        print(f"[FAIL] Orchestrator 创建失败: {e}")
        traceback.print_exc()
        return False

    # 记录执行前的临时 FITS 文件 (用于检测临时文件泄漏)
    temp_fits_before = _list_temp_fits()
    print(f"\n[检查] 执行前临时目录 FITS 文件数: {len(temp_fits_before)}")

    # 执行端到端处理
    print("\n--- 执行端到端处理 ---")
    t_start = time.time()
    try:
        result = orch.run_single(fits_path, output_dir=output_dir)
    except Exception as e:
        print(f"[FAIL] run_single 异常: {e}")
        traceback.print_exc()
        return False
    t_total = time.time() - t_start

    # 检查执行后的临时 FITS 文件
    temp_fits_after = _list_temp_fits()
    new_temp_fits = temp_fits_after - temp_fits_before
    print(f"\n[检查] 执行后临时目录 FITS 文件数: {len(temp_fits_after)}")
    if new_temp_fits:
        print(f"[FAIL] 检测到新增临时 FITS 文件 ({len(new_temp_fits)} 个):")
        for f in sorted(new_temp_fits):
            print(f"  - {f}")
    else:
        print("[OK] 无新增临时 FITS 文件 (内存管线验证通过)")

    # 验证结果
    all_pass, checks = _validate_result(result, output_dir)

    # 性能对比
    print("\n--- 性能对比 ---")
    timings = result.get("timings", {})
    # 之前的结果 (有临时文件 + Python 计算)
    prev = {"PLATESOLVE": 2.10, "PSF": 9.26, "PHOTOMETRIC": 0.96, "DRIZZLE": 15.58}
    prev_total = 27.90
    print(f"{'阶段':<15} {'之前(s)':<10} {'现在(s)':<10} {'变化':<10}")
    print("-" * 45)
    mapping = [
        ("PLATESOLVE", "platesolve"),
        ("PSF", "psf_fit"),
        ("PHOTOMETRIC", "photometric"),
        ("DRIZZLE", "drizzle"),
    ]
    now_total = 0.0
    for label, key in mapping:
        if key in timings:
            now = timings[key]
            now_total += now
            old = prev.get(label, 0)
            delta = now - old
            pct = (delta / old * 100) if old > 0 else 0
            sign = "+" if delta >= 0 else ""
            print(f"{label:<15} {old:<10.2f} {now:<10.2f} {sign}{delta:.2f}s ({sign}{pct:.1f}%)")
    print("-" * 45)
    print(f"{'总计':<15} {prev_total:<10.2f} {now_total:<10.2f} "
          f"{'+' if now_total-prev_total>=0 else ''}{now_total-prev_total:.2f}s "
          f"({'+' if (now_total-prev_total)/prev_total*100>=0 else ''}"
          f"{(now_total-prev_total)/prev_total*100:.1f}%)")
    print(f"\n  实际总耗时 (含读取): {t_total:.2f}s")

    # 最终结果
    no_temp = len(new_temp_fits) == 0
    success = all_pass and no_temp
    if success:
        print("\n[成功] 端到端测试通过! (无临时文件 + 所有验证项通过)")
    else:
        print("\n[失败] 端到端测试未通过 (查看上方详情)")

    return success


# ============================================================================
# 主入口
# ============================================================================

if __name__ == "__main__":
    print("=" * 60)
    print("端到端编排器测试 (Orchestrator E2E Test)")
    print("=" * 60)
    print(f"Python: {sys.executable}")
    print(f"项目根目录: {PROJECT_ROOT}")
    print()

    success = test_e2e()

    print()
    print("=" * 60)
    if success:
        print("测试结果: 通过")
    else:
        print("测试结果: 失败 (可能因模块/DLL/数据库不可用，查看上方详情)")
    print("=" * 60)

    sys.exit(0 if success else 1)
