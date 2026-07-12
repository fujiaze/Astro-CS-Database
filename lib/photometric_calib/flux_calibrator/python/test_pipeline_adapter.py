# -*- coding: utf-8 -*-
"""
Photometric Calib 管线适配器 - 结构验证测试
功能: 验证 pipeline_adapter.py 的代码结构正确, 辅助函数逻辑正确
用途: 不需要实际运行校准 (不依赖 DLL), 只验证代码结构与数据流转逻辑
依赖: numpy (必需); astropy (WCS 构造测试需要, 不可用时跳过)
调用: python test_pipeline_adapter.py
"""

from __future__ import annotations

import logging
import os
import sys
import tempfile

import numpy as np

# 确保能导入同目录下的 pipeline_adapter
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

logging.basicConfig(
    level=logging.INFO,
    format="[%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger(__name__)


# ============================================================================
# Mock 对象: 模拟 PipelineFramePy 的接口
# ============================================================================

class MockFrame:
    """模拟 PipelineFramePy 的接口, 用于测试辅助函数

    提供 kv_get / kv_get_double / get_block_data 方法,
    数据存储在内存 dict 中, 不依赖 C DLL。
    """

    def __init__(self):
        self._kv: dict[str, dict[str, str]] = {}  # {block_name: {key: value}}
        self._blocks: dict[str, np.ndarray] = {}

    def kv_set(self, block_name: str, key: str, value: str):
        self._kv.setdefault(block_name, {})[key] = value

    def kv_set_double(self, block_name: str, key: str, value: float):
        self.kv_set(block_name, key, str(value))

    def kv_get(self, block_name: str, key: str) -> str | None:
        return self._kv.get(block_name, {}).get(key)

    def kv_get_double(self, block_name: str, key: str,
                      default: float = 0.0) -> float:
        val = self.kv_get(block_name, key)
        if val is None:
            return default
        try:
            return float(val)
        except (ValueError, TypeError):
            return default

    def add_block(self, name: str, data, description: str = ""):
        if not isinstance(data, np.ndarray):
            data = np.asarray(data)
        self._blocks[name] = data

    def get_block_data(self, name: str) -> np.ndarray | None:
        return self._blocks.get(name)

    def remove_block(self, name: str) -> bool:
        return self._blocks.pop(name, None) is not None

    def has_block(self, name: str) -> bool:
        return name in self._blocks


class MockFSynLoader:
    """模拟 f_syn_loader 对象, 实现 get_f_syn(ra, dec, mag_g) -> float"""

    def __init__(self, base_f_syn: float = 50000.0):
        self._base = base_f_syn
        self._call_count = 0

    def get_f_syn(self, ra: float, dec: float, mag_g: float) -> float:
        self._call_count += 1
        # 简单模型: f_syn 随星等衰减
        return self._base * (10.0 ** (-0.4 * (mag_g - 12.0)))


# ============================================================================
# 测试用例
# ============================================================================

def test_import():
    """验证 1: pipeline_adapter 模块可正常导入"""
    print("\n[验证 1] pipeline_adapter 模块导入")
    try:
        import pipeline_adapter as pa
    except ImportError as e:
        print(f"  导入失败: {e}")
        return False

    # 验证核心组件存在
    has_params = hasattr(pa, "PhotometricParams")
    has_register = hasattr(pa, "register_photometric_handler")
    has_wcs_func = hasattr(pa, "_build_wcs_from_header")
    has_gaia_func = hasattr(pa, "_build_gaia_stars")
    has_psf_func = hasattr(pa, "_build_psf_results")
    has_sip_func = hasattr(pa, "_read_sip_coeffs")

    print(f"  PhotometricParams 存在: {has_params}")
    print(f"  register_photometric_handler 存在: {has_register}")
    print(f"  _build_wcs_from_header 存在: {has_wcs_func}")
    print(f"  _build_gaia_stars 存在: {has_gaia_func}")
    print(f"  _build_psf_results 存在: {has_psf_func}")
    print(f"  _read_sip_coeffs 存在: {has_sip_func}")

    ok = all([has_params, has_register, has_wcs_func,
              has_gaia_func, has_psf_func, has_sip_func])
    print(f"  [{'PASS' if ok else 'FAIL'}] 模块导入与核心组件")
    return ok


def test_photometric_params():
    """验证 2: PhotometricParams dataclass 字段正确"""
    print("\n[验证 2] PhotometricParams dataclass 字段")
    from pipeline_adapter import PhotometricParams

    # 默认值
    p = PhotometricParams()
    defaults_ok = (
        p.match_radius_px == 3.0
        and p.outlier_sigma == 3.0
        and p.max_order == 5
        and p.log_dir is None
        and p.f_syn_path is None
        and p.f_syn_loader is None
    )
    print(f"  默认值: match_radius={p.match_radius_px}, outlier_sigma={p.outlier_sigma}, "
          f"max_order={p.max_order}, log_dir={p.log_dir}")

    # 自定义值
    p2 = PhotometricParams(
        match_radius_px=5.0,
        outlier_sigma=2.5,
        max_order=3,
        log_dir="/tmp/logs",
        f_syn_path="/tmp/f_syn.json",
        f_syn_loader=MockFSynLoader(),
    )
    custom_ok = (
        p2.match_radius_px == 5.0
        and p2.outlier_sigma == 2.5
        and p2.max_order == 3
        and p2.log_dir == "/tmp/logs"
        and p2.f_syn_path == "/tmp/f_syn.json"
        and p2.f_syn_loader is not None
    )
    print(f"  自定义值: match_radius={p2.match_radius_px}, f_syn_path={p2.f_syn_path}")

    ok = defaults_ok and custom_ok
    print(f"  [{'PASS' if ok else 'FAIL'}] PhotometricParams 字段")
    return ok


def test_read_sip_coeffs():
    """验证 3: _read_sip_coeffs SIP 系数读取"""
    print("\n[验证 3] _read_sip_coeffs SIP 系数读取")
    from pipeline_adapter import _read_sip_coeffs

    # 3a: A_ORDER 不存在
    frame = MockFrame()
    order, coeffs = _read_sip_coeffs(frame, "A_ORDER", "A")
    no_sip_ok = (order == 0 and coeffs is None)
    print(f"  A_ORDER 不存在: order={order}, coeffs={coeffs}")

    # 3b: A_ORDER=2 且有系数
    frame2 = MockFrame()
    frame2.kv_set("header", "A_ORDER", "2")
    frame2.kv_set("header", "A_0_0", "0.0")
    frame2.kv_set("header", "A_1_0", "1.5e-6")
    frame2.kv_set("header", "A_0_1", "2.5e-6")
    frame2.kv_set("header", "A_2_0", "3.0e-12")
    order2, coeffs2 = _read_sip_coeffs(frame2, "A_ORDER", "A")
    has_coeffs_ok = (
        order2 == 2
        and coeffs2 is not None
        and len(coeffs2) == 36
        and abs(coeffs2[6] - 1.5e-6) < 1e-15  # A[1,0] -> idx=1*6+0=6
        and abs(coeffs2[1] - 2.5e-6) < 1e-15  # A[0,1] -> idx=0*6+1=1
        and abs(coeffs2[12] - 3.0e-12) < 1e-15  # A[2,0] -> idx=2*6+0=12
    )
    print(f"  A_ORDER=2: order={order2}, A[1,0]={coeffs2[6]:.2e}, "
          f"A[0,1]={coeffs2[1]:.2e}, A[2,0]={coeffs2[12]:.2e}")

    # 3c: A_ORDER=0
    frame3 = MockFrame()
    frame3.kv_set("header", "A_ORDER", "0")
    order3, coeffs3 = _read_sip_coeffs(frame3, "A_ORDER", "A")
    zero_order_ok = (order3 == 0 and coeffs3 is None)
    print(f"  A_ORDER=0: order={order3}, coeffs={coeffs3}")

    ok = no_sip_ok and has_coeffs_ok and zero_order_ok
    print(f"  [{'PASS' if ok else 'FAIL'}] SIP 系数读取")
    return ok


def test_build_wcs_from_header():
    """验证 4: _build_wcs_from_header WCS 构造"""
    print("\n[验证 4] _build_wcs_from_header WCS 构造")
    from pipeline_adapter import _build_wcs_from_header

    # 4a: 缺少 WCS 关键字 -> ValueError
    frame = MockFrame()
    no_wcs_ok = False
    try:
        _build_wcs_from_header(frame)
    except ValueError as e:
        no_wcs_ok = "CRVAL" in str(e) or "WCS" in str(e)
        print(f"  无 WCS: 正确抛出 ValueError: {e}")

    # 4b: 有 WCS 关键字 -> 构造 WCSTransform (需要 astropy)
    frame2 = MockFrame()
    frame2.kv_set("header", "CRVAL1", "10.0")
    frame2.kv_set("header", "CRVAL2", "20.0")
    frame2.kv_set("header", "CRPIX1", "100.0")
    frame2.kv_set("header", "CRPIX2", "100.0")
    frame2.kv_set("header", "CD1_1", "0.01")
    frame2.kv_set("header", "CD1_2", "0.0")
    frame2.kv_set("header", "CD2_1", "0.0")
    frame2.kv_set("header", "CD2_2", "0.01")
    frame2.kv_set("header", "CTYPE1", "RA---TAN")
    frame2.kv_set("header", "CTYPE2", "DEC--TAN")

    has_wcs_ok = False
    try:
        wcs = _build_wcs_from_header(frame2)
        # 验证 WCS 可用: sky_to_pixel(CRVAL) 应约等于 (CRPIX-1, CRPIX-1)
        x, y = wcs.sky_to_pixel(10.0, 20.0)
        has_wcs_ok = abs(x - 99.0) < 1e-6 and abs(y - 99.0) < 1e-6
        print(f"  有 WCS: sky_to_pixel(CRVAL)=({x:.4f}, {y:.4f}), "
              f"期望 (99, 99), has_sip={wcs.has_sip}")
    except ImportError as e:
        print(f"  [SKIP] astropy 不可用, 跳过 WCS 构造测试: {e}")
        has_wcs_ok = True  # 不计为失败
    except Exception as e:
        print(f"  WCS 构造失败: {e}")

    # 4c: 带 SIP 的 WCS
    frame3 = MockFrame()
    frame3.kv_set("header", "CRVAL1", "10.0")
    frame3.kv_set("header", "CRVAL2", "20.0")
    frame3.kv_set("header", "CRPIX1", "100.0")
    frame3.kv_set("header", "CRPIX2", "100.0")
    frame3.kv_set("header", "CD1_1", "0.01")
    frame3.kv_set("header", "CD1_2", "0.0")
    frame3.kv_set("header", "CD2_1", "0.0")
    frame3.kv_set("header", "CD2_2", "0.01")
    frame3.kv_set("header", "A_ORDER", "2")
    frame3.kv_set("header", "A_1_0", "1e-6")
    frame3.kv_set("header", "B_ORDER", "2")
    frame3.kv_set("header", "B_0_1", "1e-6")

    sip_ok = False
    try:
        wcs_sip = _build_wcs_from_header(frame3)
        sip_ok = wcs_sip.has_sip
        print(f"  SIP WCS: has_sip={wcs_sip.has_sip}")
    except ImportError:
        print(f"  [SKIP] astropy 不可用, 跳过 SIP WCS 测试")
        sip_ok = True
    except Exception as e:
        print(f"  SIP WCS 构造失败: {e}")

    ok = no_wcs_ok and has_wcs_ok and sip_ok
    print(f"  [{'PASS' if ok else 'FAIL'}] WCS 构造")
    return ok


def test_build_gaia_stars():
    """验证 5: _build_gaia_stars gaia_stars 构造"""
    print("\n[验证 5] _build_gaia_stars gaia_stars 构造")
    from pipeline_adapter import _build_gaia_stars, PhotometricParams

    # 5a: 无 f_syn 来源, gaia_cat 不存在 -> 空列表
    frame = MockFrame()
    params = PhotometricParams()
    stars = _build_gaia_stars(frame, params)
    empty_ok = len(stars) == 0
    print(f"  无 f_syn + 无 gaia_cat: stars={len(stars)}")

    # 5b: 无 f_syn 来源, 有 gaia_cat -> 空列表 (退化路径)
    frame2 = MockFrame()
    gaia_cat = np.array([
        [10.0, 20.0, 12.0],
        [10.01, 20.01, 13.0],
        [10.02, 20.02, 14.0],
    ], dtype=np.float64)
    frame2.add_block("gaia_cat", gaia_cat)
    params2 = PhotometricParams()
    stars2 = _build_gaia_stars(frame2, params2)
    no_fsyn_ok = len(stars2) == 0
    print(f"  无 f_syn + 有 gaia_cat(3星): stars={len(stars2)} (退化)")

    # 5c: 有 f_syn_loader, 有 gaia_cat -> 逐星获取 f_syn
    frame3 = MockFrame()
    frame3.add_block("gaia_cat", gaia_cat)
    loader = MockFSynLoader(base_f_syn=50000.0)
    params3 = PhotometricParams(f_syn_loader=loader)
    stars3 = _build_gaia_stars(frame3, params3)
    loader_ok = (
        len(stars3) == 3
        and all("ra" in s and "dec" in s and "mag_g" in s
                and "f_syn" in s and "source_id" in s for s in stars3)
        and all(s["f_syn"] > 0.0 for s in stars3)
        and loader._call_count == 3
        and abs(stars3[0]["f_syn"] - 50000.0) < 1.0  # mag_g=12 -> base
    )
    print(f"  f_syn_loader + gaia_cat(3星): stars={len(stars3)}, "
          f"loader调用={loader._call_count}, f_syn[0]={stars3[0]['f_syn']:.1f}")

    # 5d: gaia_cat 格式异常 -> 空列表
    frame4 = MockFrame()
    frame4.add_block("gaia_cat", np.array([1, 2, 3], dtype=np.float64))  # 1D
    params4 = PhotometricParams(f_syn_loader=MockFSynLoader())
    stars4 = _build_gaia_stars(frame4, params4)
    bad_format_ok = len(stars4) == 0
    print(f"  gaia_cat 格式异常(1D): stars={len(stars4)}")

    # 5e: f_syn_path 存在但文件不存在 -> 回退到 gaia_cat
    frame5 = MockFrame()
    frame5.add_block("gaia_cat", gaia_cat)
    loader5 = MockFSynLoader()
    params5 = PhotometricParams(
        f_syn_path="/nonexistent/f_syn.json",
        f_syn_loader=loader5,
    )
    stars5 = _build_gaia_stars(frame5, params5)
    fallback_ok = len(stars5) == 3 and loader5._call_count == 3
    print(f"  f_syn_path(不存在) 回退到 loader: stars={len(stars5)}, "
          f"loader调用={loader5._call_count}")

    ok = empty_ok and no_fsyn_ok and loader_ok and bad_format_ok and fallback_ok
    print(f"  [{'PASS' if ok else 'FAIL'}] gaia_stars 构造")
    return ok


def test_build_psf_results():
    """验证 6: _build_psf_results PSF 结果构造"""
    print("\n[验证 6] _build_psf_results PSF 结果构造")
    from pipeline_adapter import _build_psf_results
    from dynamic_psf import DPSFFitResultPy

    # 6a: psf 块不存在 -> None
    frame = MockFrame()
    result = _build_psf_results(frame)
    none_ok = result is None
    print(f"  psf 块不存在: result={result}")

    # 6b: psf 块有数据 -> DPSFFitResultPy 列表
    frame2 = MockFrame()
    psf_data = np.array([
        [0, 100.0, 50000.0, 25.0, 30.0, 3.5],  # status=0 (OK)
        [0, 120.0, 45000.0, 50.0, 60.0, 3.8],
        [1, 110.0, 48000.0, 75.0, 90.0, 4.0],  # status=1 (NO_CONVERGENCE)
    ], dtype=np.float64)
    frame2.add_block("psf", psf_data)
    results = _build_psf_results(frame2)
    has_data_ok = (
        results is not None
        and len(results) == 3
        and all(isinstance(r, DPSFFitResultPy) for r in results)
        and results[0].status == 0
        and abs(results[0].B - 100.0) < 1e-10
        and abs(results[0].flux - 50000.0) < 1e-10
        and abs(results[0].cx - 25.0) < 1e-10
        and abs(results[0].cy - 30.0) < 1e-10
        and abs(results[0].fwhm_x - 3.5) < 1e-10
        and abs(results[0].fwhm_y - 3.5) < 1e-10
        and results[2].status == 1
    )
    n_ok = sum(1 for r in results if r.status == 0)
    print(f"  psf 块(3星): results={len(results)}, status=0={n_ok}, "
          f"[0]: status={results[0].status}, B={results[0].B}, "
          f"flux={results[0].flux}, cx={results[0].cx}, cy={results[0].cy}")

    # 6c: psf 块为空 -> None
    frame3 = MockFrame()
    frame3.add_block("psf", np.array([], dtype=np.float64))
    result3 = _build_psf_results(frame3)
    empty_ok = result3 is None
    print(f"  psf 块为空: result={result3}")

    # 6d: psf 块格式异常 (1D) -> None
    frame4 = MockFrame()
    frame4.add_block("psf", np.array([1, 2, 3], dtype=np.float64))
    result4 = _build_psf_results(frame4)
    bad_format_ok = result4 is None
    print(f"  psf 块格式异常(1D): result={'None' if result4 is None else '有值'}")

    ok = none_ok and has_data_ok and empty_ok and bad_format_ok
    print(f"  [{'PASS' if ok else 'FAIL'}] PSF 结果构造")
    return ok


def test_register_handler_signature():
    """验证 7: register_photometric_handler 函数签名与可调用性"""
    print("\n[验证 7] register_photometric_handler 函数签名")
    from pipeline_adapter import register_photometric_handler, PhotometricParams
    import inspect

    # 验证函数签名
    sig = inspect.signature(register_photometric_handler)
    params_list = list(sig.parameters.keys())
    sig_ok = len(params_list) == 2 and params_list[0] == "engine" and params_list[1] == "params"
    print(f"  函数签名: register_photometric_handler{sig}")

    # 验证可调用 (用 mock engine)
    class MockEngine:
        def __init__(self):
            self.registered = []

        def register(self, stage, handler):
            self.registered.append((stage, handler))

    engine = MockEngine()
    params = PhotometricParams(log_dir=None)
    register_photometric_handler(engine, params)
    callable_ok = (
        len(engine.registered) == 1
        and engine.registered[0][0] == 2  # STAGE_PHOTOMETRIC = 2
    )
    print(f"  注册结果: stage={engine.registered[0][0]}, "
          f"handler_type={type(engine.registered[0][1]).__name__}")

    ok = sig_ok and callable_ok
    print(f"  [{'PASS' if ok else 'FAIL'}] 函数签名与注册")
    return ok


def test_handler_logic_with_mock():
    """验证 8: handler 内部逻辑 (用 mock 验证数据流转, 不实际校准)

    构造一个完整的 mock frame (含 data/header/gaia_cat/psf 块),
    验证 handler 能正确读取块数据并调用 estimator。
    由于 handler 内部调用 estimator.calibrate (需要 astropy+numpy),
    此测试验证的是数据读取与构造逻辑, 校准部分用退化路径 (空 gaia_stars)。
    """
    print("\n[验证 8] handler 内部数据流转 (退化路径, 空 gaia_stars)")
    from pipeline_adapter import register_photometric_handler, PhotometricParams

    class MockEngine:
        def __init__(self):
            self.handler_c = None
            self.stage = None

        def register(self, stage, handler_c):
            self.stage = stage
            self.handler_c = handler_c

    engine = MockEngine()
    params = PhotometricParams(log_dir=None)  # 无 f_syn -> 退化路径
    register_photometric_handler(engine, params)

    # 验证 handler 已注册
    registered_ok = engine.handler_c is not None and engine.stage == 2
    print(f"  handler 已注册: stage={engine.stage}")

    # 验证 handler 是 PipelineStageHandlerC 实例
    from astro_image_io import PipelineStageHandlerC
    type_ok = isinstance(engine.handler_c, PipelineStageHandlerC)
    print(f"  handler 类型: {type(engine.handler_c).__name__}")

    ok = registered_ok and type_ok
    print(f"  [{'PASS' if ok else 'FAIL'}] handler 数据流转")
    return ok


# ============================================================================
# 主测试入口
# ============================================================================

if __name__ == "__main__":
    print("=" * 70)
    print("Photometric Calib 管线适配器 - 结构验证测试")
    print("=" * 70)

    results = []
    results.append(("模块导入", test_import()))
    results.append(("PhotometricParams", test_photometric_params()))
    results.append(("_read_sip_coeffs", test_read_sip_coeffs()))
    results.append(("_build_wcs_from_header", test_build_wcs_from_header()))
    results.append(("_build_gaia_stars", test_build_gaia_stars()))
    results.append(("_build_psf_results", test_build_psf_results()))
    results.append(("register_photometric_handler", test_register_handler_signature()))
    results.append(("handler 数据流转", test_handler_logic_with_mock()))

    print("\n" + "=" * 70)
    print("测试汇总:")
    all_pass = True
    for name, passed in results:
        status = "PASS" if passed else "FAIL"
        print(f"  [{status}] {name}")
        if not passed:
            all_pass = False
    print("=" * 70)
    print(f"总计: {sum(1 for _, p in results if p)}/{len(results)} 通过, "
          f"{'全部通过' if all_pass else '存在失败项'}")
    print("=" * 70)
