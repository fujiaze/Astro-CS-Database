# -*- coding: utf-8 -*-
"""
test_energy_conservation.py - photometric_calib C++ DLL 能量守恒测试

功能: 验证 pc_calibrate_simple 的能量守恒性质:
    1. 像素级守恒: I_cal = I_input × scale (每像素严格成立)
    2. 匹配星流量守恒: F_cal = F_instr × scale ≈ F_syn
    3. 残差分布: N 颗匹配星 F_cal/F_syn 中位数 ≈ 1.0, MAD < 5%
    4. 退化路径: n_psf=0 / n_gaia=0 / 匹配距离 > 3px 均正确处理

用途: 作为 photometric_calib 模块的回归测试, 防止后续优化破坏能量守恒性质

运行:
    cd lib/photometric_calib/cpp/test
    python -m pytest test_energy_conservation.py -v -s

依赖:
    - photometric_calib.dll (lib/photometric_calib/cpp/ 目录下)
    - astropy (仅测试用, 用于生成参考 RA/Dec)
    - numpy
    - pytest

历史:
    2026-07-15 新建: 独立的能量守恒测试, 与 test_photometric_calib.py (功能测试) 互补
"""

from __future__ import annotations

import logging
import os
import sys
import tempfile

import numpy as np
import pytest

# ============================================================================
# 路径设置: cpp/test/ -> cpp/ -> photometric_calib/ -> python/
# ============================================================================
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_PC_PYTHON_PATH = os.path.normpath(os.path.join(_THIS_DIR, "..", "..", "python"))
if _PC_PYTHON_PATH not in sys.path:
    sys.path.insert(0, _PC_PYTHON_PATH)

# MinGW 运行时 DLL 路径 (libgomp-1.dll, zlib1.dll 等)
# gaia_client.dll 依赖 libgomp-1.dll + zlib1.dll, 这些在 MinGW bin 目录
# Python 3.8+ Windows 不用 PATH 加载 DLL, 必须用 os.add_dll_directory()
_MINGW_BIN = r"C:\msys64\mingw64\bin"
if os.path.isdir(_MINGW_BIN):
    try:
        os.add_dll_directory(_MINGW_BIN)
    except (AttributeError, OSError):
        pass

# DLL 所在目录 (photometric_calib.dll 依赖同目录的 gaia_client.dll)
_DLL_DIR = os.path.normpath(os.path.join(_THIS_DIR, ".."))
if os.path.isdir(_DLL_DIR):
    try:
        os.add_dll_directory(_DLL_DIR)
    except (AttributeError, OSError):
        if _DLL_DIR not in os.environ.get("PATH", ""):
            os.environ["PATH"] = _DLL_DIR + os.pathsep + os.environ.get("PATH", "")

# ============================================================================
# 日志配置: 输出到 %TEMP%\test_photometric.log + 控制台
# ============================================================================
_LOG_PATH = os.path.join(tempfile.gettempdir(), "test_photometric.log")
_logger = logging.getLogger("test_energy_conservation")
_logger.setLevel(logging.INFO)
# 避免重复添加 handler
if not _logger.handlers:
    _fh = logging.FileHandler(_LOG_PATH, mode="w", encoding="utf-8")
    _fh.setFormatter(logging.Formatter("[%(levelname)s] %(message)s"))
    _logger.addHandler(_fh)
    _sh = logging.StreamHandler(sys.stdout)
    _sh.setFormatter(logging.Formatter("[%(levelname)s] %(message)s"))
    _logger.addHandler(_sh)

# ============================================================================
# 检测 DLL 可用性: 加载失败则跳过全部测试 (而非 fail)
# ============================================================================
_PC_AVAILABLE = False
_PC_ERROR = ""
try:
    from photometric_calib import PhotometricCalib  # noqa: E402
    _pc = PhotometricCalib()  # 触发实际 DLL 加载
    _PC_AVAILABLE = True
except Exception as e:
    _PC_ERROR = f"{type(e).__name__}: {e}"

pytestmark = pytest.mark.skipif(
    not _PC_AVAILABLE,
    reason=f"photometric_calib DLL 加载失败, 跳过测试: {_PC_ERROR}",
)

# astropy 仅用于测试 (生成参考 RA/Dec, 与 C++ WCS 实现对比)
try:
    from astropy.wcs import WCS
    _ASTROPY_AVAILABLE = True
except ImportError:
    _ASTROPY_AVAILABLE = False

_ASTROPY_SKIP = pytest.mark.skipif(
    not _ASTROPY_AVAILABLE,
    reason="astropy 未安装, 跳过 (仅测试用依赖)",
)

# ============================================================================
# 公共 WCS 参数 (所有测试共用, 保证一致性)
# ============================================================================
_CRPIX1, _CRPIX2 = 100.0, 100.0   # 1-based FITS 约定
_CRVAL1, _CRVAL2 = 10.0, 20.0      # 度
_CD_VAL = 0.01                     # 度/像素 (~36 arcsec/pixel)
_IMG_W, _IMG_H = 200, 200


def _make_wcs(crpix1=_CRPIX1, crpix2=_CRPIX2,
              crval1=_CRVAL1, crval2=_CRVAL2,
              cd_val=_CD_VAL) -> WCS:
    """构造 TAN WCS 对象 (用于生成参考 RA/Dec)"""
    w = WCS(naxis=2)
    w.wcs.cd = [[cd_val, 0], [0, cd_val]]
    w.wcs.crval = [crval1, crval2]
    w.wcs.crpix = [crpix1, crpix2]
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    return w


def _make_stars(n_stars: int = 10, f_syn: float = 50000.0,
                flux_ratio: float = 0.1, noise: float = 0.0,
                seed: int = 42):
    """生成 n 颗测试星: 像素坐标 + Gaia RA/Dec + PSF 测光结果

    Args:
        n_stars: 星数量
        f_syn: 合成流量 F_syn (Gaia)
        flux_ratio: F_instr/F_syn 比例 (0.1 表示 F_instr = F_syn/10 → scale ≈ 10)
        noise: F_instr 的相对噪声 (0=无噪声, 0.01=1% 高斯噪声)
        seed: 随机种子

    Returns:
        (gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
         psf_cx, psf_cy, psf_flux, psf_status)
    """
    rng = np.random.default_rng(seed)
    # 像素坐标: 沿水平线分布, 距离 CRPIX 足够远避免边界
    px_vals = np.linspace(25, 175, n_stars)
    py_vals = np.full(n_stars, 100.0)
    # 生成 Gaia RA/Dec (使用 astropy WCS 保证与 C++ 投影往返一致)
    w = _make_wcs()
    world = w.all_pix2world(px_vals, py_vals, 0)
    gaia_ra = world[0]
    gaia_dec = world[1]
    gaia_mag = np.full(n_stars, 12.0)
    gaia_fsyn = np.full(n_stars, f_syn, dtype=np.float64)
    # PSF 测光: 像素位置加微小偏移 (<0.5px, 仍在 3px 匹配半径内)
    psf_cx = px_vals + 0.1
    psf_cy = py_vals - 0.1
    # F_instr = F_syn × flux_ratio × (1 + noise × N(0,1))
    psf_flux = f_syn * flux_ratio * (1.0 + noise * rng.standard_normal(n_stars))
    psf_status = np.zeros(n_stars, dtype=np.int32)
    return (gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
            psf_cx, psf_cy, psf_flux, psf_status)


def _call_calibrate(pc, image, stars, sip=None):
    """统一调用 pc.calibrate_simple, 减少重复代码

    Args:
        pc: PhotometricCalib 实例
        image: 输入图像 float32 [H, W]
        stars: _make_stars 返回的 8 元组
        sip: None 或 (sip_order, sip_a, sip_b, sip_ap, sip_bp)

    Returns:
        (out_img, n_matched, scale)
    """
    (gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
     psf_cx, psf_cy, psf_flux, psf_status) = stars
    kwargs = {}
    if sip is not None:
        sip_order, sip_a, sip_b, sip_ap, sip_bp = sip
        kwargs = dict(sip_order=sip_order, sip_a=sip_a, sip_b=sip_b,
                      sip_ap=sip_ap, sip_bp=sip_bp)
    return pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        _CRVAL1, _CRVAL2, _CRPIX1, _CRPIX2,
        _CD_VAL, 0.0, 0.0, _CD_VAL,
        **kwargs,
    )


# ============================================================================
# 测试 1: 像素级能量守恒
# ============================================================================
@_ASTROPY_SKIP
def test_pixel_level_conservation():
    """测试 1: 均匀图像 I → I×scale, 每像素严格守恒

    原理:
        C++ 实现为 I_cal[i] = I[i] * scale (全局乘法),
        因此均匀输入 I 必产生均匀输出 I×scale, 每像素偏差 < 1e-5 (相对).
    """
    _logger.info("=" * 60)
    _logger.info("[测试1] 像素级能量守恒 (均匀图像)")
    _logger.info("=" * 60)

    stars = _make_stars(n_stars=10, f_syn=50000.0, flux_ratio=0.1)
    image = np.full((_IMG_H, _IMG_W), 1000.0, dtype=np.float32)

    out_img, n_matched, scale = _call_calibrate(_pc, image, stars)

    expected = 1000.0 * scale
    _logger.info("  n_matched = %d (期望 10)", n_matched)
    _logger.info("  scale = %.6e (期望 ~10.0)", scale)
    _logger.info("  out_img[0,0] = %.6f, expected = %.6f",
                 out_img[0, 0], expected)
    _logger.info("  out_img shape = %s", out_img.shape)

    # 像素级守恒: 所有像素都等于 I_input × scale (相对容差 1e-5)
    # 注意: float32 精度限制, 用 rtol 较为合理
    assert n_matched == 10, f"匹配数 {n_matched} != 10"
    assert abs(scale - 10.0) < 0.5, f"scale {scale} 偏离 10.0 超过 0.5"
    assert out_img.shape == (_IMG_H, _IMG_W), \
        f"输出 shape {out_img.shape} != {(_IMG_H, _IMG_W)}"
    # 每像素相对误差 < 1e-5 (float32 精度范围内)
    np.testing.assert_allclose(
        out_img, expected, rtol=1e-5, atol=1e-3,
        err_msg=f"像素级守恒失败: 期望均匀 {expected}, 实际有偏差")
    # 均匀性: 最大值-最小值 应接近 0 (float32 量化误差内)
    pix_range = float(out_img.max() - out_img.min())
    _logger.info("  out_img max-min = %.6e (期望 ~0)", pix_range)
    assert pix_range < 1e-2, f"输出不均匀, max-min={pix_range}"
    _logger.info("  [PASS] 像素级守恒")


# ============================================================================
# 测试 2: 匹配星流量守恒
# ============================================================================
@_ASTROPY_SKIP
def test_matched_star_conservation():
    """测试 2: 合成 PSF + Gaia, F_cal = F_instr × scale ≈ F_syn

    原理:
        构造图像: 在 PSF 星位置放高斯星点, 总通量 = F_instr.
        调用 calibrate_simple 后, 输出图像中同一星点总通量 = F_instr × scale.
        期望: F_instr × scale ≈ F_syn (相对误差 < 1%).
    """
    _logger.info("=" * 60)
    _logger.info("[测试2] 匹配星流量守恒 (高斯 PSF)")
    _logger.info("=" * 60)

    n_stars = 10
    f_syn = 50000.0
    flux_ratio = 0.1  # F_instr = F_syn/10 → scale ≈ 10
    stars = _make_stars(n_stars=n_stars, f_syn=f_syn, flux_ratio=flux_ratio)
    (gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
     psf_cx, psf_cy, psf_flux, psf_status) = stars

    # 构造图像: 背景 + 在 PSF 星位置放高斯星点
    image = np.full((_IMG_H, _IMG_W), 100.0, dtype=np.float32)
    sigma = 1.5  # PSF 宽度 (像素)
    # 记录每颗星的理论 F_instr (用于后续对比)
    f_instr_expected = psf_flux.copy()
    # 在图像上画星 (累加高斯)
    yy, xx = np.mgrid[0:_IMG_H, 0:_IMG_W]
    for i in range(n_stars):
        cx, cy = psf_cx[i], psf_cy[i]
        gauss = np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (2 * sigma ** 2))
        # 归一化: 使总通量 = F_instr
        image += (f_instr_expected[i] * gauss / (2 * np.pi * sigma ** 2)).astype(np.float32)

    out_img, n_matched, scale = _call_calibrate(_pc, image, stars)

    _logger.info("  n_matched = %d (期望 10)", n_matched)
    _logger.info("  scale = %.6e (期望 ~10.0)", scale)

    assert n_matched == 10, f"匹配数 {n_matched} != 10"
    assert abs(scale - 10.0) < 0.5, f"scale {scale} 偏离 10.0"

    # 对每颗星做孔径测光 (半径 3σ = 4.5px → 用 5px 圆孔)
    r_ap = 5
    f_cal_arr = []
    f_syn_arr = []
    for i in range(n_stars):
        cx, cy = psf_cx[i], psf_cy[i]
        # 圆形孔径内的像素索引
        mask = (xx - cx) ** 2 + (yy - cy) ** 2 <= r_ap ** 2
        # 减去背景 (用边缘均值估计, 避开星点)
        # 简化: 直接对原始图像和输出图像做相同孔径测光
        f_in_i = float(image[mask].sum() - 100.0 * mask.sum())  # 减背景
        f_out_i = float(out_img[mask].sum() - 100.0 * scale * mask.sum())
        f_cal_arr.append(f_out_i)
        f_syn_arr.append(f_syn)
    f_cal_arr = np.array(f_cal_arr)
    f_syn_arr = np.array(f_syn_arr)

    # F_cal / F_syn 应接近 1.0 (允许孔径测光误差 + 数值精度)
    ratios = f_cal_arr / f_syn_arr
    median_ratio = float(np.median(ratios))
    _logger.info("  F_cal/F_syn 中位数 = %.4f (期望 ~1.0)", median_ratio)
    _logger.info("  F_cal/F_syn 范围 = [%.4f, %.4f]",
                 ratios.min(), ratios.max())

    # 容差 5%: 考虑孔径测光不完全 (高斯尾巴超出 r_ap=5) + scale 误差
    # 高斯 σ=1.5, r_ap=5 → 99.99% 通量在孔径内, 误差主要来自 scale
    assert abs(median_ratio - 1.0) < 0.05, \
        f"F_cal/F_syn 中位数 {median_ratio:.4f} 偏离 1.0 超过 5%"
    _logger.info("  [PASS] 匹配星流量守恒")


# ============================================================================
# 测试 3: 残差分布
# ============================================================================
@_ASTROPY_SKIP
def test_residual_distribution():
    """测试 3: N 颗匹配星 F_cal/F_syn 残差分布

    原理:
        构造 20 颗星, F_instr/F_syn 有 1% 高斯散布.
        scale = median(F_syn/F_instr) → F_cal = F_instr × scale.
        F_cal/F_syn 中位数应 ≈ 1.0, MAD < 5%.
    """
    _logger.info("=" * 60)
    _logger.info("[测试3] 残差分布 (20 颗星, 1% 通量噪声)")
    _logger.info("=" * 60)

    n_stars = 20
    f_syn = 50000.0
    flux_ratio = 0.1
    noise = 0.01  # 1% 高斯噪声
    stars = _make_stars(n_stars=n_stars, f_syn=f_syn,
                        flux_ratio=flux_ratio, noise=noise, seed=42)
    (gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
     psf_cx, psf_cy, psf_flux, psf_status) = stars

    image = np.full((_IMG_H, _IMG_W), 1000.0, dtype=np.float32)
    out_img, n_matched, scale = _call_calibrate(_pc, image, stars)

    _logger.info("  n_matched = %d (期望 20)", n_matched)
    _logger.info("  scale = %.6e (期望 ~10.0)", scale)

    assert n_matched == 20, f"匹配数 {n_matched} != 20"
    assert abs(scale - 10.0) < 0.5, f"scale {scale} 偏离 10.0"

    # 对每颗匹配星计算 F_cal/F_syn
    # F_cal = F_instr × scale (像素级校准的解析等价)
    f_cal = psf_flux * scale
    ratios = f_cal / gaia_fsyn
    median_ratio = float(np.median(ratios))
    # MAD = median(|r - median(r)|), 标准化 MAD = 1.4826 × MAD
    mad_raw = float(np.median(np.abs(ratios - median_ratio)))
    mad_normalized = 1.4826 * mad_raw

    _logger.info("  F_cal/F_syn 中位数 = %.6f (期望 ~1.0)", median_ratio)
    _logger.info("  MAD (raw) = %.6f, MAD (normalized) = %.6f",
                 mad_raw, mad_normalized)
    _logger.info("  F_cal/F_syn 范围 = [%.6f, %.6f]",
                 ratios.min(), ratios.max())

    # 中位数 ≈ 1.0 (5% 容差, 考虑 scale 是 median 估计)
    assert abs(median_ratio - 1.0) < 0.05, \
        f"F_cal/F_syn 中位数 {median_ratio:.6f} 偏离 1.0 超过 5%"
    # MAD < 5% (输入噪声 1%, 经过 median 估计后残差应在 1% 量级)
    assert mad_normalized < 0.05, \
        f"标准化 MAD {mad_normalized:.6f} 超过 5%"
    _logger.info("  [PASS] 残差分布")


# ============================================================================
# 测试 4: 退化路径
# ============================================================================
@_ASTROPY_SKIP
def test_degenerate_paths():
    """测试 4: 退化路径 (n_psf=0 / n_gaia=0 / 匹配距离过大)

    原理:
        退化情形下, C++ 应正确处理 (不崩溃, 不产生假匹配, scale=1.0).
    """
    _logger.info("=" * 60)
    _logger.info("[测试4] 退化路径 (n_psf=0 / n_gaia=0 / 距离过大)")
    _logger.info("=" * 60)

    image = np.full((50, 50), 500.0, dtype=np.float32)
    crval1, crval2 = 10.0, 20.0
    crpix1, crpix2 = 25.0, 25.0
    cd_val = 0.01

    # ---- 4a: n_psf = 0 (无 PSF 测光星) ----
    _logger.info("  [4a] n_psf=0")
    gaia_ra = np.array([10.05], dtype=np.float64)
    gaia_dec = np.array([20.05], dtype=np.float64)
    gaia_mag = np.array([12.0], dtype=np.float64)
    gaia_fsyn = np.array([50000.0], dtype=np.float64)
    psf_cx = np.array([], dtype=np.float64)
    psf_cy = np.array([], dtype=np.float64)
    psf_flux = np.array([], dtype=np.float64)
    psf_status = np.array([], dtype=np.int32)

    out_img, n_matched, scale = _pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
    )
    _logger.info("    n_matched=%d, scale=%.6e, out[0,0]=%.4f",
                 n_matched, scale, out_img[0, 0])
    assert n_matched == 0, f"n_psf=0 时 n_matched 应为 0, 实际 {n_matched}"
    assert abs(scale - 1.0) < 1e-9, f"n_psf=0 时 scale 应为 1.0, 实际 {scale}"
    # 输出 = 输入 × 1.0 = 输入
    np.testing.assert_allclose(out_img, image, rtol=1e-6,
                               err_msg="n_psf=0 时输出应等于输入")

    # ---- 4b: n_gaia = 0 (无 Gaia 参考星) ----
    _logger.info("  [4b] n_gaia=0")
    gaia_ra = np.array([], dtype=np.float64)
    gaia_dec = np.array([], dtype=np.float64)
    gaia_mag = np.array([], dtype=np.float64)
    gaia_fsyn = np.array([], dtype=np.float64)
    psf_cx = np.array([10.0], dtype=np.float64)
    psf_cy = np.array([10.0], dtype=np.float64)
    psf_flux = np.array([100.0], dtype=np.float64)
    psf_status = np.array([0], dtype=np.int32)

    out_img, n_matched, scale = _pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
    )
    _logger.info("    n_matched=%d, scale=%.6e, out[0,0]=%.4f",
                 n_matched, scale, out_img[0, 0])
    assert n_matched == 0, f"n_gaia=0 时 n_matched 应为 0, 实际 {n_matched}"
    assert abs(scale - 1.0) < 1e-9, f"n_gaia=0 时 scale 应为 1.0, 实际 {scale}"
    np.testing.assert_allclose(out_img, image, rtol=1e-6,
                               err_msg="n_gaia=0 时输出应等于输入")

    # ---- 4c: PSF 星与 Gaia 星距离 > 3px (无法匹配) ----
    _logger.info("  [4c] 匹配距离 > 3px")
    # Gaia 星投影到像素 (25, 25), PSF 星在 (50, 50), 距离 ≈ 35px > 3px
    gaia_ra = np.array([crval1], dtype=np.float64)
    gaia_dec = np.array([crval2], dtype=np.float64)
    gaia_mag = np.array([12.0], dtype=np.float64)
    gaia_fsyn = np.array([50000.0], dtype=np.float64)
    psf_cx = np.array([50.0], dtype=np.float64)
    psf_cy = np.array([50.0], dtype=np.float64)
    psf_flux = np.array([5000.0], dtype=np.float64)
    psf_status = np.array([0], dtype=np.int32)

    out_img, n_matched, scale = _pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
    )
    _logger.info("    n_matched=%d, scale=%.6e, out[0,0]=%.4f",
                 n_matched, scale, out_img[0, 0])
    assert n_matched == 0, \
        f"距离 > 3px 时 n_matched 应为 0, 实际 {n_matched}"
    assert abs(scale - 1.0) < 1e-9, \
        f"无匹配时 scale 应为 1.0, 实际 {scale}"
    np.testing.assert_allclose(out_img, image, rtol=1e-6,
                               err_msg="无匹配时输出应等于输入")

    _logger.info("  [PASS] 退化路径")


# ============================================================================
# 主入口: 直接运行 (非 pytest) 时执行所有测试
# ============================================================================
if __name__ == "__main__":
    print("=" * 60)
    print("photometric_calib 能量守恒测试")
    print(f"日志: {_LOG_PATH}")
    print("=" * 60)

    if not _PC_AVAILABLE:
        print(f"[SKIP] DLL 不可用: {_PC_ERROR}")
        sys.exit(0)

    tests = [
        ("像素级能量守恒", test_pixel_level_conservation),
        ("匹配星流量守恒", test_matched_star_conservation),
        ("残差分布", test_residual_distribution),
        ("退化路径", test_degenerate_paths),
    ]
    results = []
    for name, fn in tests:
        try:
            fn()
            results.append((name, True))
        except Exception as e:
            _logger.error("[FAIL] %s 异常: %s", name, e)
            import traceback
            traceback.print_exc()
            results.append((name, False))

    print("\n" + "=" * 60)
    print("测试汇总:")
    n_pass = sum(1 for _, ok in results if ok)
    for name, ok in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")
    print(f"\n总计: {n_pass}/{len(results)} 通过")
    print("=" * 60)
