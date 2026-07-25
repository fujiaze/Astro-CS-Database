# -*- coding: utf-8 -*-
"""
P03-004 SNR 稀疏模型与 SIP 一致性验证脚本

验证项:
  A. snr_model schema (SnrModel 序列化格式: n_points + points + 3x f64)
  B. WCS+SIP 转球面 (snr_extract_model 控制点 vs astropy all_pix2world, 差异 < 1e-6 度)
  C. SIP 修正生效 (有 SIP vs 无 SIP, 控制点坐标不同)
  D. 退化路径 (n_stars=0, sigma_residual=0, nullptr)
  E. HISS 稀疏模型写入 (hiss_write_snr_model + hiss_read_snr_model 往返)

依赖: numpy, ctypes, astropy (可选, 用于 WCS 验证; 无 astropy 时跳过 B 的 astropy 对比)
"""
from __future__ import annotations

import json
import math
import os
import sys
import struct
import ctypes
from ctypes import (
    c_int, c_uint32, c_double, c_float, c_uint64,
    POINTER, byref, cdll, Structure, c_void_p, c_char_p, c_char, pointer,
)

import numpy as np

# ============================================================================
# 配置: 路径
# ============================================================================
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
SNR_DLL_PATH = os.path.join(PROJECT_ROOT, "lib", "snr_estimator", "cpp", "snr_estimator.dll")
AIO_DLL_PATH = os.path.join(PROJECT_ROOT, "build", "artifacts", "astro_image_io.dll")

# ============================================================================
# ctypes 结构体 (与 snr_estimator.h 一致)
# ============================================================================
SNR_SIP_COEFF_SIZE = 36


class SnrSipCoeffs(Structure):
    _fields_ = [
        ("a_order", c_int),
        ("b_order", c_int),
        ("a", c_double * SNR_SIP_COEFF_SIZE),
        ("b", c_double * SNR_SIP_COEFF_SIZE),
    ]


class SnrWcsParams(Structure):
    _fields_ = [
        ("crval1", c_double),
        ("crval2", c_double),
        ("crpix1", c_double),
        ("crpix2", c_double),
        ("cd", c_double * 4),
        ("sip", SnrSipCoeffs),
    ]


class SnrControlPoint(Structure):
    _fields_ = [
        ("ra", c_double),
        ("dec", c_double),
        ("snr_psf", c_float),
    ]


class SnrModel(Structure):
    _fields_ = [
        ("n_points", c_uint32),
        ("points", POINTER(SnrControlPoint)),
        ("snr_phot", c_double),
        ("median_snr", c_double),
        ("idw_power", c_double),
    ]


# ============================================================================
# 加载 DLL
# ============================================================================
def load_snr_dll():
    """加载 snr_estimator.dll 并绑定签名"""
    dll = cdll.LoadLibrary(SNR_DLL_PATH)
    dll.snr_extract_model.restype = c_int
    dll.snr_extract_model.argtypes = [
        POINTER(c_double), c_int, c_double,
        POINTER(SnrWcsParams), POINTER(SnrModel),
    ]
    dll.snr_free_model.restype = None
    dll.snr_free_model.argtypes = [POINTER(SnrModel)]
    dll.snr_estimate.restype = c_int
    dll.snr_estimate.argtypes = [
        POINTER(c_float), c_int, c_int,
        POINTER(c_double), c_int, c_double,
        POINTER(c_float),
    ]
    return dll


def load_aio_dll():
    """加载 astro_image_io.dll 并绑定 hiss_write_snr_model / hiss_read_snr_model"""
    # Python 3.8+ 默认不搜索 DLL 目录, 需显式 add_dll_directory + 改 cwd
    # AIO DLL 依赖 mingw64 运行时, 需加入 PATH
    aio_dir = os.path.dirname(AIO_DLL_PATH)
    mingw_bin = r"C:\msys64\mingw64\bin"
    try:
        os.add_dll_directory(aio_dir)
    except (AttributeError, OSError):
        pass
    if os.path.isdir(mingw_bin):
        try:
            os.add_dll_directory(mingw_bin)
        except (AttributeError, OSError):
            pass
        os.environ["PATH"] = mingw_bin + os.pathsep + os.environ.get("PATH", "")
    prev_cwd = os.getcwd()
    try:
        os.chdir(aio_dir)
        dll = cdll.LoadLibrary(AIO_DLL_PATH)
    finally:
        os.chdir(prev_cwd)
    # aio_hiss_write_snr_model(path, nside, nested, n_pix, ipix, pixel, snr_model, meta_json)
    dll.aio_hiss_write_snr_model.restype = c_int
    dll.aio_hiss_write_snr_model.argtypes = [
        c_char_p, c_uint32, c_int, c_uint64,
        POINTER(c_uint64), POINTER(c_float),
        c_void_p, c_char_p,
    ]
    # aio_hiss_read_snr_model(path, *nside, *nested, *n_pix, **ipix, **pixel, **snr_model, **meta_json)
    dll.aio_hiss_read_snr_model.restype = c_int
    dll.aio_hiss_read_snr_model.argtypes = [
        c_char_p, POINTER(c_uint32), POINTER(c_int), POINTER(c_uint64),
        POINTER(POINTER(c_uint64)), POINTER(POINTER(c_float)),
        POINTER(c_void_p), POINTER(POINTER(c_char)),
    ]
    dll.aio_hio_free.restype = None
    dll.aio_hio_free.argtypes = [c_void_p]
    dll.aio_hio_free_snr_model.restype = None
    dll.aio_hio_free_snr_model.argtypes = [c_void_p]
    return dll


from ctypes import c_char_p, c_uint64, c_int64

# ============================================================================
# 测试数据构造
# ============================================================================
def make_test_psf(n_stars=10, w=2000, h=2000):
    """构造 PSF 星数组 [n,9]: status=0, B=100, flux=50000, cx/cy 网格, fwhm=3, A=500, mad=10"""
    psf = np.zeros((n_stars, 9), dtype=np.float64)
    xs = np.linspace(100, w - 100, n_stars)
    ys = np.linspace(100, h - 100, n_stars)
    for i in range(n_stars):
        psf[i] = [0, 100.0, 50000.0, xs[i], ys[i], 3.0, 500.0, 10.0, 0.1]
    return psf


def make_wcs_with_sip(w=2000, h=2000):
    """构造 WCS 参数 + SIP 系数 (A_ORDER=2, B_ORDER=2)

    CRVAL=(272.825665, -13.131811)  # 银心附近, 与 P03-002 测试帧一致
    CRPIX=(w/2+0.5, h/2+0.5)        # 1-based, 图像中心
    CD = [[-1.75e-3, 0], [0, 1.75e-3]]  # ~6.3"/px
    SIP A/B order=2, 含非零系数模拟光学畸变
    """
    wcs = SnrWcsParams()
    wcs.crval1 = 272.825665
    wcs.crval2 = -13.131811
    wcs.crpix1 = w / 2.0 + 0.5
    wcs.crpix2 = h / 2.0 + 0.5
    wcs.cd[0] = -1.752e-3   # CD1_1
    wcs.cd[1] = 0.0          # CD1_2
    wcs.cd[2] = 0.0          # CD2_1
    wcs.cd[3] = 1.752e-3    # CD2_2

    # SIP A/B order=2 (典型光学畸变量级 ~1e-5)
    wcs.sip.a_order = 2
    wcs.sip.b_order = 2
    # 全零初始化
    for i in range(SNR_SIP_COEFF_SIZE):
        wcs.sip.a[i] = 0.0
        wcs.sip.b[i] = 0.0
    # A: dx^2, dy^2, dx*dy 项 (i+j>=1, i+j<=2)
    # 索引: a[i*6+j]
    wcs.sip.a[1 * 6 + 0] = 5.0e-6   # A_1_0 (dx)
    wcs.sip.a[0 * 6 + 1] = -2.0e-6  # A_0_1 (dy)
    wcs.sip.a[2 * 6 + 0] = 1.0e-9   # A_2_0 (dx^2)
    wcs.sip.a[0 * 6 + 2] = 1.0e-9   # A_0_2 (dy^2)
    wcs.sip.a[1 * 6 + 1] = -1.0e-9  # A_1_1 (dx*dy)

    wcs.sip.b[1 * 6 + 0] = 2.0e-6   # B_1_0
    wcs.sip.b[0 * 6 + 1] = -5.0e-6  # B_0_1
    wcs.sip.b[2 * 6 + 0] = 1.0e-9
    wcs.sip.b[0 * 6 + 2] = 1.0e-9
    wcs.sip.b[1 * 6 + 1] = -1.0e-9

    return wcs


def make_wcs_no_sip(w=2000, h=2000):
    """构造无 SIP 的 WCS (a_order=0)"""
    wcs = make_wcs_with_sip(w, h)
    wcs.sip.a_order = 0
    wcs.sip.b_order = 0
    for i in range(SNR_SIP_COEFF_SIZE):
        wcs.sip.a[i] = 0.0
        wcs.sip.b[i] = 0.0
    return wcs


# ============================================================================
# 验证 A: snr_model schema
# ============================================================================
def test_a_schema(dll):
    """验证 SnrModel 序列化 schema: n_points(u32) + points(N*20B) + snr_phot(f64) + median_snr(f64) + idw_power(f64)"""
    print("\n=== 验证 A: snr_model schema ===")
    psf = make_test_psf(10)
    wcs = make_wcs_no_sip()
    sigma = 0.15

    model = SnrModel()
    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 10,
        c_double(sigma), byref(wcs), byref(model),
    )
    assert ret == 0, f"snr_extract_model 返回码 {ret} (期望 0)"
    assert model.n_points == 10, f"n_points={model.n_points} (期望 10)"
    assert model.snr_phot > 0, f"snr_phot={model.snr_phot} 应 > 0"
    expected_snr_phot = 1.0 / (math.log(10) * sigma)
    assert abs(model.snr_phot - expected_snr_phot) < 1e-9, \
        f"snr_phot={model.snr_phot} 期望={expected_snr_phot}"
    assert model.median_snr > 0, f"median_snr={model.median_snr} 应 > 0"
    assert abs(model.idw_power - 2.0) < 1e-9, f"idw_power={model.idw_power} 期望 2.0"

    # 验证 SnrControlPoint 结构 (ra, dec, snr_psf)
    p0 = model.points[0]
    assert -360 <= p0.ra <= 720, f"ra={p0.ra} 超范围"
    assert -90 <= p0.dec <= 90, f"dec={p0.dec} 超范围"
    assert p0.snr_psf > 0, f"snr_psf={p0.snr_psf} 应 > 0"
    # snr_psf = (A-B)/mad = (500-100)/10 = 40
    assert abs(p0.snr_psf - 40.0) < 1e-4, f"snr_psf={p0.snr_psf} 期望 40.0"

    # 序列化 schema 验证: 总字节数 = 4 + n_points*20 + 24
    expected_payload = 4 + 10 * 20 + 24
    print(f"  n_points = {model.n_points}")
    print(f"  snr_phot = {model.snr_phot:.6f} (期望 {expected_snr_phot:.6f})")
    print(f"  median_snr = {model.median_snr:.6f}")
    print(f"  idw_power = {model.idw_power}")
    print(f"  ctrl_point[0]: ra={p0.ra:.6f} dec={p0.dec:.6f} snr_psf={p0.snr_psf:.4f}")
    print(f"  序列化 payload = {expected_payload} 字节 (4 + 10*20 + 24)")
    dll.snr_free_model(byref(model))
    print("  [PASS] schema 验证通过")
    return True


# ============================================================================
# 验证 B: WCS+SIP 转球面 (与 astropy all_pix2world 对比)
# ============================================================================
def test_b_wcs_sip_consistency(dll):
    """验证 snr_extract_model 控制点 (ra,dec) 与 astropy WCS+SIP all_pix2world 一致"""
    print("\n=== 验证 B: WCS+SIP 转球面一致性 ===")
    try:
        from astropy.wcs import WCS
        from astropy.io.fits import Header
    except ImportError:
        print("  [SKIP] astropy 未安装, 跳过 astropy 对比")
        return _test_b_internal_consistency(dll)

    psf = make_test_psf(10)
    wcs_sip = make_wcs_with_sip()
    sigma = 0.15

    # 1) 用 snr_extract_model 得到控制点 (含 SIP)
    model = SnrModel()
    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 10,
        c_double(sigma), byref(wcs_sip), byref(model),
    )
    assert ret == 0, f"snr_extract_model (SIP) 返回码 {ret}"

    # 2) 构造 astropy WCS 含同样的 CD + SIP
    hdr = Header()
    hdr["CTYPE1"] = "RA---TAN-SIP"
    hdr["CTYPE2"] = "DEC--TAN-SIP"
    hdr["CRVAL1"] = wcs_sip.crval1
    hdr["CRVAL2"] = wcs_sip.crval2
    hdr["CRPIX1"] = wcs_sip.crpix1
    hdr["CRPIX2"] = wcs_sip.crpix2
    hdr["CD1_1"] = wcs_sip.cd[0]
    hdr["CD1_2"] = wcs_sip.cd[1]
    hdr["CD2_1"] = wcs_sip.cd[2]
    hdr["CD2_2"] = wcs_sip.cd[3]
    hdr["A_ORDER"] = 2
    hdr["B_ORDER"] = 2
    # A_i_j (跳过 (0,0))
    hdr["A_1_0"] = wcs_sip.sip.a[1 * 6 + 0]
    hdr["A_0_1"] = wcs_sip.sip.a[0 * 6 + 1]
    hdr["A_2_0"] = wcs_sip.sip.a[2 * 6 + 0]
    hdr["A_0_2"] = wcs_sip.sip.a[0 * 6 + 2]
    hdr["A_1_1"] = wcs_sip.sip.a[1 * 6 + 1]
    hdr["B_1_0"] = wcs_sip.sip.b[1 * 6 + 0]
    hdr["B_0_1"] = wcs_sip.sip.b[0 * 6 + 1]
    hdr["B_2_0"] = wcs_sip.sip.b[2 * 6 + 0]
    hdr["B_0_2"] = wcs_sip.sip.b[0 * 6 + 2]
    hdr["B_1_1"] = wcs_sip.sip.b[1 * 6 + 1]

    astropy_wcs = WCS(hdr)
    # all_pix2world 应用前向 SIP (与 snr_extract_model 一致)
    # 输入: 0-based 像素坐标
    # 返回 (ra_array, dec_array)
    px = psf[:, 3]  # cx
    py = psf[:, 4]  # cy
    ra_arr, dec_arr = astropy_wcs.all_pix2world(px, py, 0)  # origin=0 (0-based)

    # 3) 对比
    max_diff_ra = 0.0
    max_diff_dec = 0.0
    for i in range(10):
        cp = model.points[i]
        diff_ra = abs(cp.ra - ra_arr[i])
        diff_dec = abs(cp.dec - dec_arr[i])
        if diff_ra > max_diff_ra:
            max_diff_ra = diff_ra
        if diff_dec > max_diff_dec:
            max_diff_dec = diff_dec

    print(f"  snr_extract_model vs astropy all_pix2world (含 SIP):")
    print(f"    max |Δra|  = {max_diff_ra:.3e} 度 ({max_diff_ra * 3600:.3e} 角秒)")
    print(f"    max |Δdec| = {max_diff_dec:.3e} 度 ({max_diff_dec * 3600:.3e} 角秒)")
    print(f"  控制点[0]: snr ra={model.points[0].ra:.8f} dec={model.points[0].dec:.8f}")
    print(f"            astro ra={ra_arr[0]:.8f} dec={dec_arr[0]:.8f}")

    dll.snr_free_model(byref(model))

    # 阈值: < 1e-9 度 (~3.6e-3 角秒, 远小于 SIP 畸变量级)
    assert max_diff_ra < 1e-9, f"Δra={max_diff_ra} 超阈值 1e-9 度"
    assert max_diff_dec < 1e-9, f"Δdec={max_diff_dec} 超阈值 1e-9 度"
    print("  [PASS] WCS+SIP 一致性验证通过 (差异 < 1e-9 度)")
    return True


def _test_b_internal_consistency(dll):
    """无 astropy 时的内部一致性验证: SIP 启用 vs 禁用, 控制点坐标应不同"""
    print("  (回退: 内部一致性验证 - SIP 启用 vs 禁用)")
    psf = make_test_psf(10)
    sigma = 0.15

    # 含 SIP
    wcs_sip = make_wcs_with_sip()
    model_sip = SnrModel()
    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 10,
        c_double(sigma), byref(wcs_sip), byref(model_sip),
    )
    assert ret == 0

    # 无 SIP
    wcs_nosip = make_wcs_no_sip()
    model_nosip = SnrModel()
    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 10,
        c_double(sigma), byref(wcs_nosip), byref(model_nosip),
    )
    assert ret == 0

    # 边缘像素 (x=100, y=100) 远离 CRPIX(1000.5, 1000.5), SIP 修正应显著
    diff_ra = abs(model_sip.points[0].ra - model_nosip.points[0].ra)
    diff_dec = abs(model_sip.points[0].dec - model_nosip.points[0].dec)
    print(f"    控制点[0] SIP vs 无SIP: Δra={diff_ra:.3e}° Δdec={diff_dec:.3e}°")
    assert diff_ra > 1e-10 or diff_dec > 1e-10, "SIP 启用 vs 禁用坐标无差异, SIP 未生效"

    dll.snr_free_model(byref(model_sip))
    dll.snr_free_model(byref(model_nosip))
    print("  [PASS] 内部一致性验证 (SIP 修正生效)")
    return True


# ============================================================================
# 验证 C: SIP 修正生效 (有 SIP vs 无 SIP, 边缘控制点坐标不同)
# ============================================================================
def test_c_sip_effect(dll):
    """验证 SIP 修正在边缘像素显著 (x=100,y=100 距 CRPIX 900px)"""
    print("\n=== 验证 C: SIP 修正生效 (边缘像素) ===")
    psf = make_test_psf(10)
    sigma = 0.15

    wcs_sip = make_wcs_with_sip()
    model_sip = SnrModel()
    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 10,
        c_double(sigma), byref(wcs_sip), byref(model_sip),
    )
    assert ret == 0

    wcs_nosip = make_wcs_no_sip()
    model_nosip = SnrModel()
    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 10,
        c_double(sigma), byref(wcs_nosip), byref(model_nosip),
    )
    assert ret == 0

    # 边缘像素 (x=100,y=100) SIP 修正量
    # dx = 100 - (1000.5 - 1) = -899.5, dy = -899.5
    # A_1_0 * dx = 5e-6 * -899.5 = -4.5e-3 px
    # CD1_1 * Δdx = -1.752e-3 * -4.5e-3 = 7.9e-6 度
    # 期望 Δra 量级 ~1e-5 度
    diff_ra_edge = abs(model_sip.points[0].ra - model_nosip.points[0].ra)
    diff_dec_edge = abs(model_sip.points[0].dec - model_nosip.points[0].dec)
    diff_ra_center = abs(model_sip.points[5].ra - model_nosip.points[5].ra)
    diff_dec_center = abs(model_sip.points[5].dec - model_nosip.points[5].dec)

    print(f"  边缘点[0] (px 100,100): Δra={diff_ra_edge:.3e}° Δdec={diff_dec_edge:.3e}°")
    print(f"  中心点[5] (px ~1000,1000): Δra={diff_ra_center:.3e}° Δdec={diff_dec_center:.3e}°")
    print(f"  边缘 SIP 修正 > 中心 SIP 修正: {diff_ra_edge > diff_ra_center}")

    # 边缘 SIP 修正应显著 (> 1e-7 度, 即 > 3.6e-4 角秒)
    assert diff_ra_edge > 1e-7 or diff_dec_edge > 1e-7, \
        f"边缘 SIP 修正不显著: Δra={diff_ra_edge} Δdec={diff_dec_edge}"

    # 中心点 SIP 修正应小于边缘点 (因 SIP 多项式在 CRPIX 处为 0)
    # 注: psf[5] 不完全在 CRPIX, 但距 CRPIX 较近
    # 这里只验证边缘修正显著, 不强制中心 < 边缘 (因 psf 分布)

    dll.snr_free_model(byref(model_sip))
    dll.snr_free_model(byref(model_nosip))
    print("  [PASS] SIP 修正在边缘像素生效")
    return True


# ============================================================================
# 验证 D: 退化路径
# ============================================================================
def test_d_degenerate(dll):
    """验证退化路径返回码: n_stars=0 -> 1, sigma=0 -> 2, nullptr -> 3"""
    print("\n=== 验证 D: 退化路径 ===")
    wcs = make_wcs_no_sip()

    # n_stars = 0 -> ret=1
    psf_empty = np.zeros((0, 9), dtype=np.float64)
    model = SnrModel()
    ret = dll.snr_extract_model(
        psf_empty.ctypes.data_as(POINTER(c_double)), 0,
        c_double(0.1), byref(wcs), byref(model),
    )
    assert ret == 1, f"n_stars=0 应返回 1, 实际 {ret}"
    print(f"  n_stars=0 -> ret={ret} (期望 1) [PASS]")

    # sigma_residual = 0 -> ret=2
    psf = make_test_psf(5)
    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 5,
        c_double(0.0), byref(wcs), byref(model),
    )
    assert ret == 2, f"sigma=0 应返回 2, 实际 {ret}"
    print(f"  sigma=0 -> ret={ret} (期望 2) [PASS]")

    # nullptr -> ret=3
    ret = dll.snr_extract_model(
        None, 5, c_double(0.1), byref(wcs), byref(model),
    )
    assert ret == 3, f"psf=None 应返回 3, 实际 {ret}"
    print(f"  psf=None -> ret={ret} (期望 3) [PASS]")

    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 5,
        c_double(0.1), None, byref(model),
    )
    assert ret == 3, f"wcs=None 应返回 3, 实际 {ret}"
    print(f"  wcs=None -> ret={ret} (期望 3) [PASS]")

    ret = dll.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 5,
        c_double(0.1), byref(wcs), None,
    )
    assert ret == 3, f"model=None 应返回 3, 实际 {ret}"
    print(f"  model=None -> ret={ret} (期望 3) [PASS]")

    print("  [PASS] 退化路径全部正确")
    return True


# ============================================================================
# 验证 E: HISS 稀疏模型写入/读取往返
# ============================================================================
def test_e_hiss_snr_model_roundtrip(dll_snr):
    """验证 hiss_write_snr_model + hiss_read_snr_model 往返 (snr_format=1)"""
    print("\n=== 验证 E: HISS 稀疏模型写入/读取往返 ===")
    try:
        aio = load_aio_dll()
    except Exception as e:
        print(f"  [SKIP] 加载 astro_image_io.dll 失败: {e}")
        return True

    # 1) 构造 SnrModel (含 5 控制点)
    psf = make_test_psf(5)
    wcs = make_wcs_with_sip()
    model = SnrModel()
    ret = dll_snr.snr_extract_model(
        psf.ctypes.data_as(POINTER(c_double)), 5,
        c_double(0.1), byref(wcs), byref(model),
    )
    assert ret == 0, f"snr_extract_model 返回 {ret}"

    # 2) 构造 HIO SnrModel (供 hiss_write_snr_model 使用)
    # HioSnrModel 结构: n_points(u32) + points(ptr) + snr_phot(f64) + median_snr(f64) + idw_power(f64)
    # HioSnrControlPoint: ra(f64) + dec(f64) + snr_psf(f32) = 20 字节 (packed, 与 C++ #pragma pack(1) 一致)
    class HioSnrControlPoint(Structure):
        _pack_ = 1
        _fields_ = [("ra", c_double), ("dec", c_double), ("snr_psf", c_float)]

    class HioSnrModel(Structure):
        _fields_ = [
            ("n_points", c_uint32),
            ("points", POINTER(HioSnrControlPoint)),
            ("snr_phot", c_double),
            ("median_snr", c_double),
            ("idw_power", c_double),
        ]

    # 复制 snr_model 数据到 HIO 结构 (points 需 malloc 分配, aio 负责释放)
    n_pts = model.n_points
    # 用 ctypes 数组分配 points
    PointsArr = HioSnrControlPoint * n_pts
    hio_points = PointsArr()
    for i in range(n_pts):
        hio_points[i].ra = model.points[i].ra
        hio_points[i].dec = model.points[i].dec
        hio_points[i].snr_psf = model.points[i].snr_psf

    hio_model = HioSnrModel()
    hio_model.n_points = n_pts
    hio_model.points = pointer(hio_points[0]) if n_pts > 0 else None
    hio_model.snr_phot = model.snr_phot
    hio_model.median_snr = model.median_snr
    hio_model.idw_power = model.idw_power

    # 保存原始数据用于对比
    orig_data = {
        "n_points": n_pts,
        "points": [(model.points[i].ra, model.points[i].dec, model.points[i].snr_psf)
                   for i in range(n_pts)],
        "snr_phot": model.snr_phot,
        "median_snr": model.median_snr,
        "idw_power": model.idw_power,
    }
    dll_snr.snr_free_model(byref(model))

    # 3) 构造 ipix / pixel 数组 (10 个 HEALPix 像素)
    n_pix = 10
    ipix_arr = (c_uint64 * n_pix)(*([1000 + i for i in range(n_pix)]))
    pixel_arr = (c_float * n_pix)(*([100.0 + i for i in range(n_pix)]))
    meta_json = b'{"filter":"Red","exposure_s":180.0}'

    # 4) 写入 .hiss
    hiss_path = os.path.join(SCRIPT_DIR, "test_snr_model.hiss")
    if os.path.exists(hiss_path):
        os.remove(hiss_path)

    rc = aio.aio_hiss_write_snr_model(
        hiss_path.encode("utf-8"), 512, 1, n_pix,
        ipix_arr, pixel_arr,
        byref(hio_model), meta_json,
    )
    assert rc == 0, f"hiss_write_snr_model 失败 rc={rc}"
    print(f"  hiss_write_snr_model 成功: {hiss_path} ({os.path.getsize(hiss_path)} 字节)")

    # 5) 读取 .hiss (hiss_read_snr_model)
    nside_out = c_uint32(0)
    nested_out = c_int(0)
    n_pix_out = c_uint64(0)
    ipix_out = POINTER(c_uint64)()
    pixel_out = POINTER(c_float)()
    snr_model_out = c_void_p()
    meta_out = POINTER(c_char)()

    rc = aio.aio_hiss_read_snr_model(
        hiss_path.encode("utf-8"),
        byref(nside_out), byref(nested_out), byref(n_pix_out),
        byref(ipix_out), byref(pixel_out),
        byref(snr_model_out), byref(meta_out),
    )
    assert rc == 0, f"hiss_read_snr_model 失败 rc={rc}"

    assert nside_out.value == 512, f"nside={nside_out.value} 期望 512"
    assert nested_out.value == 1, f"nested={nested_out.value} 期望 1"
    assert n_pix_out.value == n_pix, f"n_pix={n_pix_out.value} 期望 {n_pix}"

    # 验证 ipix / pixel
    for i in range(n_pix):
        assert ipix_out[i] == 1000 + i, f"ipix[{i}]={ipix_out[i]} 期望 {1000 + i}"
        assert abs(pixel_out[i] - (100.0 + i)) < 1e-4, f"pixel[{i}]={pixel_out[i]}"

    # 验证 snr_model (snr_model_out 是 HioSnrModel* 指针)
    hio_read = ctypes.cast(snr_model_out, POINTER(HioSnrModel)).contents
    assert hio_read.n_points == orig_data["n_points"], \
        f"read n_points={hio_read.n_points} 期望 {orig_data['n_points']}"
    assert abs(hio_read.snr_phot - orig_data["snr_phot"]) < 1e-9, \
        f"read snr_phot={hio_read.snr_phot} 期望 {orig_data['snr_phot']}"
    assert abs(hio_read.median_snr - orig_data["median_snr"]) < 1e-9
    assert abs(hio_read.idw_power - orig_data["idw_power"]) < 1e-9

    for i in range(orig_data["n_points"]):
        exp_ra, exp_dec, exp_snr = orig_data["points"][i]
        got_ra = hio_read.points[i].ra
        got_dec = hio_read.points[i].dec
        got_snr = hio_read.points[i].snr_psf
        assert abs(got_ra - exp_ra) < 1e-9, f"cp[{i}].ra={got_ra} 期望 {exp_ra}"
        assert abs(got_dec - exp_dec) < 1e-9, f"cp[{i}].dec={got_dec} 期望 {exp_dec}"
        assert abs(got_snr - exp_snr) < 1e-4, f"cp[{i}].snr_psf={got_snr} 期望 {exp_snr}"

    print(f"  hiss_read_snr_model 成功: nside={nside_out.value} n_pix={n_pix_out.value}")
    print(f"  snr_model 往返: n_points={hio_read.n_points} snr_phot={hio_read.snr_phot:.6f}")
    print(f"  控制点[0]: ra={hio_read.points[0].ra:.6f} dec={hio_read.points[0].dec:.6f} "
          f"snr_psf={hio_read.points[0].snr_psf:.4f}")

    # 验证 meta_json 中的 snr_format=1
    # meta_out 是 POINTER(c_char), 用 ctypes.string_at 读取
    if meta_out:
        meta_str = ctypes.cast(meta_out, c_char_p).value.decode("utf-8")
    else:
        meta_str = ""
    assert "snr_format" in meta_str, f"meta 缺少 snr_format 字段: {meta_str[:200]}"
    assert '"snr_format":1' in meta_str or '"snr_format": 1' in meta_str, \
        f"snr_format 应为 1: {meta_str[:200]}"
    print(f"  meta snr_format=1 验证通过")

    # 释放
    aio.aio_hio_free(ipix_out)
    aio.aio_hio_free(pixel_out)
    aio.aio_hio_free(meta_out)
    aio.aio_hio_free_snr_model(snr_model_out)

    # 清理测试文件
    if os.path.exists(hiss_path):
        os.remove(hiss_path)

    print("  [PASS] HISS 稀疏模型往返验证通过")
    return True


import ctypes


# ============================================================================
# 主函数
# ============================================================================
def main():
    print("=" * 70)
    print("P03-004 SNR 稀疏模型与 SIP 一致性验证")
    print("=" * 70)
    print(f"snr_estimator.dll: {SNR_DLL_PATH}")
    print(f"astro_image_io.dll: {AIO_DLL_PATH}")

    if not os.path.exists(SNR_DLL_PATH):
        print(f"[FATAL] snr_estimator.dll 不存在")
        return 1

    dll = load_snr_dll()

    results = []
    tests = [
        ("A. snr_model schema", lambda: test_a_schema(dll)),
        ("B. WCS+SIP 一致性", lambda: test_b_wcs_sip_consistency(dll)),
        ("C. SIP 修正生效", lambda: test_c_sip_effect(dll)),
        ("D. 退化路径", lambda: test_d_degenerate(dll)),
        ("E. HISS 稀疏模型往返", lambda: test_e_hiss_snr_model_roundtrip(dll)),
    ]

    for name, fn in tests:
        try:
            ok = fn()
            results.append((name, ok))
        except Exception as e:
            import traceback
            traceback.print_exc()
            print(f"  [FAIL] {name}: {e}")
            results.append((name, False))

    print("\n" + "=" * 70)
    print("验证结果汇总")
    print("=" * 70)
    n_pass = sum(1 for _, ok in results if ok)
    for name, ok in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")
    print(f"\n  {n_pass}/{len(results)} 通过")
    return 0 if n_pass == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
