# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

# -*- coding: utf-8 -*-
"""
test_spectrum_integrator_golden.py - P12-003 光谱积分与响应曲线无回归测试

用途:
  1. 验证 spectrum_integrator.cpp 的 compute_f_syn / compute_f_syn_cached 数值
     与 Python 参考实现 SyntheticPhotometry.compute 一致 (算法等价性)
  2. 验证 P12-002 (star_matcher.cpp 修改) 未引入回归
  3. 生成 filter/QE provenance 报告

策略:
  - 用 Planck 黑体合成 5 条 Gaia BP/RP uint8 光谱 (T=3500/4500/5800/7500/10000K)
  - 加载 filters.json 与 qe_curves.json 的多条曲线
  - 对每条光谱 × 每个 filter × (有QE / 无QE):
      Python: SyntheticPhotometry.compute (0.1nm 步长)
      C++:   test_spectrum_integrator.exe (compute_f_syn 1nm + compute_f_syn_cached spectrum_wl)
  - 比对 |F_cpp - F_python| / F_python < tolerance
  - 容差: 1.0% (理由: 步长不同 0.1 vs 1 vs 2nm, Python scipy.Akima vs C++ 自实现 Akima 略有差异)

依赖: numpy, scipy, pytest (可选), photometric_calib.python.synthetic_photometry
编译: 需先编译 test_spectrum_integrator.exe (见 cpp/build.ps1)

调用:
  python test_spectrum_integrator_golden.py
  pytest test_spectrum_integrator_golden.py -v
"""

from __future__ import annotations

import json
import logging
import math
import os
import subprocess
import sys
import tempfile
from typing import Optional

import numpy as np

# 配置路径
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_PC_ROOT = os.path.normpath(os.path.join(_THIS_DIR, "..", ".."))  # lib/photometric_calib/
_PYTHON_PATH = os.path.join(_PC_ROOT, "python")
_DATA_DIR = os.path.join(_PC_ROOT, "data", "response_curves")
_CPP_DIR = os.path.join(_PC_ROOT, "cpp")
_EXE_PATH = os.path.join(_CPP_DIR, "test", "test_spectrum_integrator.exe")

if _PYTHON_PATH not in sys.path:
    sys.path.insert(0, _PYTHON_PATH)

# C++ exe 依赖 MinGW 运行时 DLL (libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll)
# 显式注入 PATH, 避免 subprocess 调用时 DLL 找不到
_MINGW_BIN = r"C:\msys64\mingw64\bin"
if os.path.isdir(_MINGW_BIN):
    os.environ["Path"] = _MINGW_BIN + ";" + os.environ.get("Path", "")

logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
logger = logging.getLogger(__name__)


# ============================================================================
# 物理常数 (SI, 与 C++ test_spectrum_integrator.cpp 完全一致)
# ============================================================================
PLANCK_H = 6.62607015e-34   # J·s
SPEED_C = 2.99792458e8       # m/s
BOLTZ_K = 1.380649e-23      # J/K


def planck_radiance(wl_nm: float, T_kelvin: float) -> float:
    """Planck 黑体辐射 B(λ, T) - 与 C++ 完全一致"""
    wl_m = wl_nm * 1e-9
    wl5 = wl_m ** 5
    exponent = (PLANCK_H * SPEED_C) / (wl_m * BOLTZ_K * T_kelvin)
    if exponent > 700.0:
        return 0.0
    denom = math.exp(exponent) - 1.0
    if denom <= 0.0:
        return 0.0
    return (2.0 * PLANCK_H * SPEED_C * SPEED_C) / (wl5 * denom)


def make_blackbody_spectrum_uint8(T_kelvin: float) -> tuple[np.ndarray, np.ndarray]:
    """构造 Gaia BP/RP uint8 光谱 (343 点, 336~1020nm, 步长 2nm)

    与 C++ test_spectrum_integrator.cpp make_blackbody_spectrum 完全一致:
    按最大值归一化到 [0, 255]

    Returns:
        (spectrum_wl[nm], spectrum_uint8)
    """
    n_points = 343
    wl_start = 336.0
    wl_step = 2.0

    wl = np.array([wl_start + i * wl_step for i in range(n_points)], dtype=np.float64)
    b_vals = np.array([planck_radiance(w, T_kelvin) for w in wl], dtype=np.float64)
    b_max = b_vals.max()

    if b_max <= 0.0:
        return wl, np.zeros(n_points, dtype=np.uint8)

    norm = (b_vals / b_max) * 255.0
    norm = np.clip(norm, 0.0, 255.0)
    spec = np.round(norm).astype(np.uint8)
    return wl, spec


# ============================================================================
# 数据加载 (filters.json / qe_curves.json)
# ============================================================================
def load_curve_json(path: str, name: str) -> tuple[np.ndarray, np.ndarray]:
    """从 JSON 加载指定名称的曲线

    Returns:
        (wavelength_nm, value)
    """
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    if name not in data:
        raise KeyError(f"曲线 '{name}' 不存在于 {path}, 可用: {list(data.keys())}")
    entry = data[name]
    wl = np.asarray(entry["wavelength_nm"], dtype=np.float64)
    val = np.asarray(entry["value"], dtype=np.float64)
    return wl, val


def list_curves_json(path: str) -> list[str]:
    """列出 JSON 中所有曲线名称"""
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return list(data.keys())


def write_curve_txt(path: str, wl: np.ndarray, val: np.ndarray) -> None:
    """写曲线到简单文本文件 (与 C++ read_wl_val_file 一致)"""
    n = len(wl)
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"{n}\n")
        for w, v in zip(wl, val):
            f.write(f"{w:.6f} {v:.10f}\n")


# ============================================================================
# Python 参考实现 (调用 SyntheticPhotometry.compute, 0.1nm 步长)
# ============================================================================
def python_reference_f_syn(
    spec_uint8: np.ndarray,
    spec_wl: np.ndarray,
    filter_wl: np.ndarray,
    filter_trans: np.ndarray,
    qe_wl: Optional[np.ndarray],
    qe_trans: Optional[np.ndarray],
    mag_g: float,
) -> float:
    """Python 参考实现: 用 SyntheticPhotometry.compute 计算 F_syn

    步长 0.1nm (与 SyntheticPhotometry 默认一致)
    """
    from synthetic_photometry import SyntheticPhotometry

    sed_flux = spec_uint8.astype(np.float64) * (10.0 ** (-0.4 * mag_g))
    f_syn = SyntheticPhotometry.compute(
        spec_wl, sed_flux,
        filter_wl, filter_trans,
        qe_wl, qe_trans,
        wl_step=0.1,
    )
    return float(f_syn)


# ============================================================================
# C++ test exe 调用
# ============================================================================
def cpp_test_f_syn(
    filter_wl: np.ndarray,
    filter_trans: np.ndarray,
    qe_wl: Optional[np.ndarray],
    qe_trans: Optional[np.ndarray],
    mag_g: float,
) -> list[dict]:
    """调用 C++ test_spectrum_integrator.exe 获取 F_syn

    Returns:
        list of dict, 每元素 {T_kelvin, mag_g, spec_count, f_syn_uncached, f_syn_cached}
    """
    if not os.path.isfile(_EXE_PATH):
        raise FileNotFoundError(
            f"C++ test exe 未找到: {_EXE_PATH}\n"
            f"请先编译: 在 {_CPP_DIR} 下运行 build_test_spectrum_integrator.ps1 或手动 g++"
        )

    with tempfile.TemporaryDirectory() as tmpdir:
        filter_path = os.path.join(tmpdir, "filter.txt")
        write_curve_txt(filter_path, filter_wl, filter_trans)

        if qe_wl is not None and qe_trans is not None:
            qe_path = os.path.join(tmpdir, "qe.txt")
            write_curve_txt(qe_path, qe_wl, qe_trans)
            qe_arg = qe_path
        else:
            qe_arg = "none"

        cmd = [_EXE_PATH, filter_path, qe_arg, str(mag_g)]
        logger.info("调用 C++ exe: %s", " ".join(cmd))
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60, encoding="utf-8")
        if proc.returncode != 0:
            raise RuntimeError(
                f"C++ exe 失败 (exit={proc.returncode}):\n"
                f"stderr: {proc.stderr}\nstdout: {proc.stdout}"
            )
        try:
            results = json.loads(proc.stdout)
        except json.JSONDecodeError as e:
            raise RuntimeError(
                f"C++ exe stdout 解析 JSON 失败: {e}\n"
                f"stdout: {proc.stdout}\nstderr: {proc.stderr}"
            )

    logger.info("C++ exe 返回 %d 条结果", len(results))
    if proc.stderr:
        # 仅供调试, 不影响测试结果
        for line in proc.stderr.strip().splitlines()[:5]:
            logger.debug("[C++ stderr] %s", line)
    return results


# ============================================================================
# Golden 比对测试
# ============================================================================
def test_filter_qe_provenance():
    """验证 filter/QE 曲线数据可加载且元数据完整"""
    print("=" * 60)
    print("[测试1] filter/QE provenance (filters.json + qe_curves.json)")
    print("=" * 60)

    filters_path = os.path.join(_DATA_DIR, "filters.json")
    qe_path = os.path.join(_DATA_DIR, "qe_curves.json")

    assert os.path.isfile(filters_path), f"filters.json 不存在: {filters_path}"
    assert os.path.isfile(qe_path), f"qe_curves.json 不存在: {qe_path}"

    filter_names = list_curves_json(filters_path)
    qe_names = list_curves_json(qe_path)
    print(f"  滤光片曲线数: {len(filter_names)}")
    print(f"  QE 曲线数:    {len(qe_names)}")
    print(f"  滤光片名称: {filter_names}")
    print(f"  QE 名称:    {qe_names}")

    # 验证每条曲线的数据完整性
    provenance = {"filters": {}, "qe": {}}
    for name in filter_names:
        wl, val = load_curve_json(filters_path, name)
        assert len(wl) == len(val), f"{name}: wl/val 长度不一致 {len(wl)} vs {len(val)}"
        assert len(wl) >= 2, f"{name}: 点数 < 2"
        assert np.all(np.diff(wl) > 0), f"{name}: 波长非严格递增"
        assert np.all((val >= 0) & (val <= 1.0)), f"{name}: 值超出 [0,1] 范围"
        prov = {
            "n_points": int(len(wl)),
            "wl_min": float(wl[0]),
            "wl_max": float(wl[-1]),
            "val_min": float(val.min()),
            "val_max": float(val.max()),
        }
        provenance["filters"][name] = prov
        print(f"  filter '{name}': {prov['n_points']} 点, "
              f"范围 [{prov['wl_min']:.1f}, {prov['wl_max']:.1f}] nm, "
              f"val [{prov['val_min']:.3f}, {prov['val_max']:.3f}]")

    for name in qe_names:
        wl, val = load_curve_json(qe_path, name)
        assert len(wl) == len(val), f"{name}: wl/val 长度不一致"
        assert len(wl) >= 2, f"{name}: 点数 < 2"
        assert np.all(np.diff(wl) > 0), f"{name}: 波长非严格递增"
        assert np.all((val >= 0) & (val <= 1.0)), f"{name}: 值超出 [0,1] 范围"
        prov = {
            "n_points": int(len(wl)),
            "wl_min": float(wl[0]),
            "wl_max": float(wl[-1]),
            "val_min": float(val.min()),
            "val_max": float(val.max()),
        }
        provenance["qe"][name] = prov
        print(f"  QE '{name}': {prov['n_points']} 点, "
              f"范围 [{prov['wl_min']:.1f}, {prov['wl_max']:.1f}] nm, "
              f"val [{prov['val_min']:.3f}, {prov['val_max']:.3f}]")

    # 保存 provenance 到 JSON (供证据文件引用)
    prov_path = os.path.join(_THIS_DIR, "filter_qe_provenance.json")
    with open(prov_path, "w", encoding="utf-8") as f:
        json.dump(provenance, f, ensure_ascii=False, indent=2)
    print(f"\n  provenance 已保存: {prov_path}")

    # 至少 1 个滤光片 + 1 个 QE
    ok = len(filter_names) >= 1 and len(qe_names) >= 1
    print(f"\n  [{'PASS' if ok else 'FAIL'}] filter/QE provenance")
    return ok, provenance


def test_cpp_python_fsyn_consistency():
    """验证 C++ compute_f_syn / compute_f_syn_cached 与 Python SyntheticPhotometry.compute 一致

    对 5 条黑体光谱 × N 个 filter × (有 QE / 无 QE) × mag_g 组合:
      |F_cpp - F_python| / F_python < 1.0% (无 QE)
      |F_cpp - F_python| / F_python < 1.0% (有 QE)

    容差理由: Python 用 0.1nm 步长 + scipy.Akima, C++ 用 1nm/2nm 步长 + 自实现 Akima.
    """
    print("\n" + "=" * 60)
    print("[测试2] C++ vs Python F_syn 数值一致性 (5 光谱 × 多 filter × 有/无 QE)")
    print("=" * 60)

    if not os.path.isfile(_EXE_PATH):
        print(f"  [SKIP] C++ exe 未找到: {_EXE_PATH}")
        return False, []

    filters_path = os.path.join(_DATA_DIR, "filters.json")
    qe_path = os.path.join(_DATA_DIR, "qe_curves.json")
    filter_names = list_curves_json(filters_path)
    qe_names = list_curves_json(qe_path)

    # 选取测试组合 (避免过长: 第一个 filter + 一个宽带 filter + 第一个 QE)
    test_filter_names = [filter_names[0]]
    if len(filter_names) > 1:
        test_filter_names.append(filter_names[-1])
    test_qe_name = qe_names[0] if qe_names else None
    test_mag_g_values = [10.0, 12.0, 15.0]

    # 5 条黑体光谱
    temps = [3500.0, 4500.0, 5800.0, 7500.0, 10000.0]
    specs = [(T, *make_blackbody_spectrum_uint8(T)) for T in temps]
    print(f"  合成光谱: {len(specs)} 条 (T={temps} K)")
    print(f"  测试 filter: {test_filter_names}")
    print(f"  测试 QE: {test_qe_name}")
    print(f"  测试 mag_g: {test_mag_g_values}")

    results_log = []
    n_pass = 0
    n_total = 0
    tolerance = 0.01  # 1.0% 相对误差容差

    for filter_name in test_filter_names:
        filter_wl, filter_trans = load_curve_json(filters_path, filter_name)

        for has_qe in [False, True]:
            if has_qe and test_qe_name is None:
                continue
            if has_qe:
                qe_wl, qe_trans = load_curve_json(qe_path, test_qe_name)
            else:
                qe_wl = qe_trans = None

            for mag_g in test_mag_g_values:
                # ---- Python 参考值 ----
                py_results = []
                for T, spec_wl, spec_uint8 in specs:
                    f_py = python_reference_f_syn(
                        spec_uint8, spec_wl,
                        filter_wl, filter_trans,
                        qe_wl, qe_trans,
                        mag_g,
                    )
                    py_results.append({"T_kelvin": T, "f_syn_python": f_py})

                # ---- C++ 测试值 ----
                cpp_results = cpp_test_f_syn(
                    filter_wl, filter_trans, qe_wl, qe_trans, mag_g
                )

                # ---- 比对 ----
                for py_r, cpp_r in zip(py_results, cpp_results):
                    assert py_r["T_kelvin"] == cpp_r["T_kelvin"], \
                        f"T 不匹配: py={py_r['T_kelvin']} vs cpp={cpp_r['T_kelvin']}"
                    f_py = py_r["f_syn_python"]
                    f_cpp_uncached = cpp_r["f_syn_uncached"]
                    f_cpp_cached = cpp_r["f_syn_cached"]
                    T = py_r["T_kelvin"]

                    if f_py <= 0:
                        print(f"  [WARN] T={T}K mag={mag_g} filter={filter_name} QE={has_qe}: "
                              f"F_python={f_py:.4e} <= 0, 跳过比对")
                        continue

                    rel_uncached = abs(f_cpp_uncached - f_py) / f_py
                    rel_cached = abs(f_cpp_cached - f_py) / f_py
                    rel_cached_vs_uncached = abs(f_cpp_cached - f_cpp_uncached) / max(f_cpp_uncached, 1e-30)

                    n_total += 1
                    pass_uncached = rel_uncached < tolerance
                    pass_cached = rel_cached < tolerance
                    if pass_uncached and pass_cached:
                        n_pass += 1
                        status = "PASS"
                    else:
                        status = "FAIL"

                    print(f"  [{status}] T={T:.0f}K mag={mag_g:.1f} filter='{filter_name}' QE={has_qe}: "
                          f"F_py={f_py:.4e} F_cpp_unc={f_cpp_uncached:.4e} F_cpp_cached={f_cpp_cached:.4e} "
                          f"rel_unc={rel_uncached*100:.3f}% rel_cached={rel_cached*100:.3f}%")

                    results_log.append({
                        "T_kelvin": T,
                        "mag_g": mag_g,
                        "filter_name": filter_name,
                        "has_qe": has_qe,
                        "qe_name": test_qe_name if has_qe else None,
                        "f_syn_python": f_py,
                        "f_syn_cpp_uncached": f_cpp_uncached,
                        "f_syn_cpp_cached": f_cpp_cached,
                        "rel_err_uncached": rel_uncached,
                        "rel_err_cached": rel_cached,
                        "rel_err_cached_vs_uncached": rel_cached_vs_uncached,
                        "status": status,
                    })

    # 统计
    print(f"\n  比对统计: {n_pass}/{n_total} 通过 (容差 {tolerance*100:.1f}%)")
    if results_log:
        rel_unc_values = [r["rel_err_uncached"] for r in results_log]
        rel_cached_values = [r["rel_err_cached"] for r in results_log]
        print(f"  无缓存版相对误差: min={min(rel_unc_values)*100:.4f}% "
              f"max={max(rel_unc_values)*100:.4f}% "
              f"median={sorted(rel_unc_values)[len(rel_unc_values)//2]*100:.4f}%")
        print(f"  缓存版相对误差:   min={min(rel_cached_values)*100:.4f}% "
              f"max={max(rel_cached_values)*100:.4f}% "
              f"median={sorted(rel_cached_values)[len(rel_cached_values)//2]*100:.4f}%")

    ok = n_pass == n_total and n_total > 0
    print(f"\n  [{'PASS' if ok else 'FAIL'}] C++ vs Python F_syn 一致性")
    return ok, results_log


def test_cached_vs_uncached_consistency():
    """验证 C++ compute_f_syn (1nm 步长) vs compute_f_syn_cached (spectrum_wl 网格)

    由于积分步长不同 (1nm vs 2nm spectrum_wl), 数值会有微小差异但应在容差内.
    """
    print("\n" + "=" * 60)
    print("[测试3] C++ cached (2nm) vs uncached (1nm) 一致性")
    print("=" * 60)

    if not os.path.isfile(_EXE_PATH):
        print(f"  [SKIP] C++ exe 未找到: {_EXE_PATH}")
        return False, []

    # 复用测试2的 cpp 结果 (但需要重新跑, 因为只看 cached vs uncached)
    filters_path = os.path.join(_DATA_DIR, "filters.json")
    qe_path = os.path.join(_DATA_DIR, "qe_curves.json")
    filter_names = list_curves_json(filters_path)
    qe_names = list_curves_json(qe_path)

    test_filter_names = [filter_names[0]]
    if len(filter_names) > 1:
        test_filter_names.append(filter_names[-1])
    test_qe_name = qe_names[0] if qe_names else None
    test_mag_g_values = [10.0, 12.0, 15.0]

    tolerance = 0.05  # 5% 容差 (步长差异更大: 1nm vs 2nm)
    results_log = []
    n_pass = 0
    n_total = 0

    for filter_name in test_filter_names:
        filter_wl, filter_trans = load_curve_json(filters_path, filter_name)
        for has_qe in [False, True]:
            if has_qe and test_qe_name is None:
                continue
            if has_qe:
                qe_wl, qe_trans = load_curve_json(qe_path, test_qe_name)
            else:
                qe_wl = qe_trans = None

            for mag_g in test_mag_g_values:
                cpp_results = cpp_test_f_syn(filter_wl, filter_trans, qe_wl, qe_trans, mag_g)
                for r in cpp_results:
                    f_unc = r["f_syn_uncached"]
                    f_cached = r["f_syn_cached"]
                    if f_unc <= 0:
                        continue
                    rel = abs(f_cached - f_unc) / f_unc
                    n_total += 1
                    status = "PASS" if rel < tolerance else "FAIL"
                    if status == "PASS":
                        n_pass += 1
                    print(f"  [{status}] T={r['T_kelvin']:.0f}K mag={mag_g:.1f} "
                          f"filter='{filter_name}' QE={has_qe}: "
                          f"uncached={f_unc:.4e} cached={f_cached:.4e} "
                          f"rel={rel*100:.3f}%")
                    results_log.append({
                        "T_kelvin": r["T_kelvin"],
                        "mag_g": mag_g,
                        "filter_name": filter_name,
                        "has_qe": has_qe,
                        "f_syn_uncached": f_unc,
                        "f_syn_cached": f_cached,
                        "rel_err": rel,
                        "status": status,
                    })

    print(f"\n  比对统计: {n_pass}/{n_total} 通过 (容差 {tolerance*100:.1f}%)")
    ok = n_pass == n_total and n_total > 0
    print(f"\n  [{'PASS' if ok else 'FAIL'}] C++ cached vs uncached 一致性")
    return ok, results_log


def test_no_qe_vs_qe1():
    """验证无 QE 与 QE=1.0 数值一致 (向后兼容性)

    Python: SyntheticPhotometry.compute(qe_wl=None) vs (qe_wl=sw, qe_val=ones)
    C++:    compute_f_syn(qe_wl=nullptr) vs (qe_wl=sw, qe_trans=ones)
    """
    print("\n" + "=" * 60)
    print("[测试4] 无 QE vs QE=1.0 一致性")
    print("=" * 60)

    if not os.path.isfile(_EXE_PATH):
        print(f"  [SKIP] C++ exe 未找到: {_EXE_PATH}")
        return False, []

    filters_path = os.path.join(_DATA_DIR, "filters.json")
    filter_names = list_curves_json(filters_path)
    test_filter_name = filter_names[0]
    filter_wl, filter_trans = load_curve_json(filters_path, test_filter_name)

    mag_g = 12.0

    # 构造 QE=1 的曲线 (覆盖光谱范围)
    spec_wl, spec_uint8 = make_blackbody_spectrum_uint8(5800.0)
    qe_wl_ones = spec_wl.copy()
    qe_val_ones = np.ones_like(spec_wl)

    # ---- C++ 测试 ----
    # 1. 无 QE
    cpp_no_qe = cpp_test_f_syn(filter_wl, filter_trans, None, None, mag_g)
    # 2. QE=1
    cpp_qe1 = cpp_test_f_syn(filter_wl, filter_trans, qe_wl_ones, qe_val_ones, mag_g)

    # 比对 5800K (index 2)
    cpp_no_qe_5800 = next(r for r in cpp_no_qe if r["T_kelvin"] == 5800.0)
    cpp_qe1_5800 = next(r for r in cpp_qe1 if r["T_kelvin"] == 5800.0)

    print(f"  C++ 无 QE (T=5800K): uncached={cpp_no_qe_5800['f_syn_uncached']:.6e}, "
          f"cached={cpp_no_qe_5800['f_syn_cached']:.6e}")
    print(f"  C++ QE=1 (T=5800K):  uncached={cpp_qe1_5800['f_syn_uncached']:.6e}, "
          f"cached={cpp_qe1_5800['f_syn_cached']:.6e}")

    rel_unc = abs(cpp_no_qe_5800['f_syn_uncached'] - cpp_qe1_5800['f_syn_uncached']) / max(cpp_no_qe_5800['f_syn_uncached'], 1e-30)
    rel_cached = abs(cpp_no_qe_5800['f_syn_cached'] - cpp_qe1_5800['f_syn_cached']) / max(cpp_no_qe_5800['f_syn_cached'], 1e-30)

    # 容差: 1e-9 (理论上应严格相等, 但浮点误差可能引入微小差异)
    tolerance = 1e-9
    pass_unc = rel_unc < tolerance
    pass_cached = rel_cached < tolerance
    print(f"  无 QE vs QE=1 相对误差: uncached={rel_unc:.2e}, cached={rel_cached:.2e}")

    ok = pass_unc and pass_cached
    print(f"\n  [{'PASS' if ok else 'FAIL'}] 无 QE == QE=1 (容差 {tolerance:.0e})")
    return ok, {
        "cpp_no_qe": cpp_no_qe_5800,
        "cpp_qe1": cpp_qe1_5800,
        "rel_err_uncached": rel_unc,
        "rel_err_cached": rel_cached,
    }


def test_p12_002_no_regression_in_photometric_calib():
    """运行现有 5 个 photometric_calib 测试, 验证 P12-002 修改后无回归

    通过调用 test_photometric_calib.py 实现.
    """
    print("\n" + "=" * 60)
    print("[测试5] P12-002 修改后 photometric_calib 现有测试无回归")
    print("=" * 60)

    test_script = os.path.join(_CPP_DIR, "test", "test_photometric_calib.py")
    if not os.path.isfile(test_script):
        print(f"  [SKIP] test_photometric_calib.py 不存在")
        return False, {}

    cmd = [sys.executable, test_script]
    print(f"  调用: {' '.join(cmd)}")
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120, encoding="utf-8")
    print("  ---- stdout ----")
    for line in proc.stdout.splitlines()[-30:]:
        print(f"  {line}")
    if proc.stderr:
        print("  ---- stderr (前 5 行) ----")
        for line in proc.stderr.splitlines()[:5]:
            print(f"  {line}")

    exit_code = proc.returncode
    # 解析 "总计: N/M 通过"
    n_pass = 0
    n_total = 0
    for line in proc.stdout.splitlines():
        if "总计:" in line and "通过" in line:
            try:
                parts = line.split("总计:")[1].split("通过")[0].strip().split("/")
                n_pass = int(parts[0])
                n_total = int(parts[1])
            except Exception:
                pass

    ok = (exit_code == 0 and n_pass == n_total and n_total == 5)
    print(f"\n  exit_code={exit_code}, 测试 {n_pass}/{n_total} 通过")
    print(f"  [{'PASS' if ok else 'FAIL'}] photometric_calib 现有测试无回归")
    return ok, {
        "exit_code": exit_code,
        "n_pass": n_pass,
        "n_total": n_total,
        "stdout_tail": proc.stdout[-2000:] if proc.stdout else "",
    }


# ============================================================================
# 主程序入口
# ============================================================================
if __name__ == "__main__":
    print("=" * 60)
    print("P12-003 光谱积分与响应曲线无回归测试")
    print("=" * 60)
    print(f"exe: {_EXE_PATH}")
    print(f"data: {_DATA_DIR}")
    print(f"python: {_PYTHON_PATH}")

    all_results = []
    all_logs = {}

    # 测试1: filter/QE provenance
    try:
        ok, provenance = test_filter_qe_provenance()
        all_results.append(("filter/QE provenance", ok))
        all_logs["test1_provenance"] = provenance
    except Exception as e:
        print(f"  [FAIL] 异常: {e}")
        import traceback
        traceback.print_exc()
        all_results.append(("filter/QE provenance", False))
        all_logs["test1_provenance"] = {"error": str(e)}

    # 测试2: C++ vs Python F_syn 一致性
    try:
        ok, fsyn_log = test_cpp_python_fsyn_consistency()
        all_results.append(("C++ vs Python F_syn", ok))
        all_logs["test2_fsyn_consistency"] = fsyn_log
    except Exception as e:
        print(f"  [FAIL] 异常: {e}")
        import traceback
        traceback.print_exc()
        all_results.append(("C++ vs Python F_syn", False))
        all_logs["test2_fsyn_consistency"] = {"error": str(e)}

    # 测试3: C++ cached vs uncached
    try:
        ok, cached_log = test_cached_vs_uncached_consistency()
        all_results.append(("C++ cached vs uncached", ok))
        all_logs["test3_cached_vs_uncached"] = cached_log
    except Exception as e:
        print(f"  [FAIL] 异常: {e}")
        import traceback
        traceback.print_exc()
        all_results.append(("C++ cached vs uncached", False))
        all_logs["test3_cached_vs_uncached"] = {"error": str(e)}

    # 测试4: 无 QE vs QE=1
    try:
        ok, qe_log = test_no_qe_vs_qe1()
        all_results.append(("无 QE vs QE=1", ok))
        all_logs["test4_no_qe_vs_qe1"] = qe_log
    except Exception as e:
        print(f"  [FAIL] 异常: {e}")
        import traceback
        traceback.print_exc()
        all_results.append(("无 QE vs QE=1", False))
        all_logs["test4_no_qe_vs_qe1"] = {"error": str(e)}

    # 测试5: photometric_calib 现有测试
    try:
        ok, reg_log = test_p12_002_no_regression_in_photometric_calib()
        all_results.append(("photometric_calib 现有测试", ok))
        all_logs["test5_regression"] = reg_log
    except Exception as e:
        print(f"  [FAIL] 异常: {e}")
        import traceback
        traceback.print_exc()
        all_results.append(("photometric_calib 现有测试", False))
        all_logs["test5_regression"] = {"error": str(e)}

    # ---- 保存完整日志到 JSON ----
    log_path = os.path.join(_THIS_DIR, "test_spectrum_integrator_golden_results.json")
    with open(log_path, "w", encoding="utf-8") as f:
        json.dump(all_logs, f, ensure_ascii=False, indent=2, default=str)
    print(f"\n完整测试日志已保存: {log_path}")

    # ---- 汇总 ----
    print("\n" + "=" * 60)
    print("测试汇总:")
    n_pass = sum(1 for _, ok in all_results if ok)
    for name, ok in all_results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")
    print(f"\n总计: {n_pass}/{len(all_results)} 通过")
    print("=" * 60)

    sys.exit(0 if n_pass == len(all_results) else 1)
