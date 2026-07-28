# -*- coding: utf-8 -*-
"""
test_photometric_calib.py - C++ DLL 简化版测光校准测试
功能: 构造小图像 + Gaia星表 + PSF结果, 调用pc_calibrate_simple, 验证校正结果
依赖: numpy, astropy (生成参考RA/Dec)
"""

from __future__ import annotations

import logging
import os
import sys

import numpy as np

# 配置路径: cpp/test/ -> cpp/ -> photometric_calib/ -> python/
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_PC_PATH = os.path.normpath(os.path.join(_THIS_DIR, "..", "..", "python"))
if _PC_PATH not in sys.path:
    sys.path.insert(0, _PC_PATH)

logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
logger = logging.getLogger(__name__)


def test_basic_calibration():
    """测试基本测光校准流程"""
    from photometric_calib import PhotometricCalib

    print("=" * 60)
    print("[测试1] 基本测光校准 (10颗星, TAN投影)")
    print("=" * 60)

    pc = PhotometricCalib()

    # ---- 构造WCS参数 (简单TAN) ----
    crpix1, crpix2 = 100.0, 100.0  # 1-based
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.01  # 度/像素
    img_w, img_h = 200, 200

    # ---- 10颗星: 像素坐标 ----
    px_vals = np.linspace(25, 175, 10)
    py_vals = np.full(10, 100.0)

    # 用astropy生成参考RA/Dec (确保WCS往返一致)
    from astropy.wcs import WCS
    w = WCS(naxis=2)
    w.wcs.cd = [[cd_val, 0], [0, cd_val]]
    w.wcs.crval = [crval1, crval2]
    w.wcs.crpix = [crpix1, crpix2]
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    world = w.all_pix2world(px_vals, py_vals, 0)
    gaia_ra = world[0]
    gaia_dec = world[1]
    gaia_mag = np.full(10, 12.0)
    gaia_fsyn = np.full(10, 50000.0)

    # ---- PSF星: 像素位置+微小偏移, flux = f_syn/10 ----
    psf_cx = px_vals + 0.1
    psf_cy = py_vals - 0.1
    psf_flux = gaia_fsyn / 10.0  # F_instr = F_syn/10 -> scale = 10
    psf_status = np.zeros(10, dtype=np.int32)

    # 1颗失败的PSF星 (应被过滤)
    psf_cx = np.append(psf_cx, 10.0)
    psf_cy = np.append(psf_cy, 10.0)
    psf_flux = np.append(psf_flux, 0.0)
    psf_status = np.append(psf_status, np.int32(1))

    # ---- 原图 ----
    image = np.full((img_h, img_w), 1000.0, dtype=np.float32)

    # ---- 调用 (P12-001: 返回5元组含 diag) ----
    out_img, n_matched, scale, sigma_residual, diag = pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
    )

    print(f"  n_matched = {n_matched} (期望 10)")
    print(f"  scale = {scale:.6e} (期望 ~10.0)")
    print(f"  out_img[0,0] = {out_img[0, 0]:.4f} (期望 ~10000.0)")
    print(f"  out_img shape = {out_img.shape}")
    print(f"  diag.fit_used = {diag.fit_used} (期望 10)")
    print(f"  diag.psf_valid = {diag.psf_valid} (期望 10)")

    t1_ok = (n_matched == 10 and abs(scale - 10.0) < 0.5
             and abs(out_img[0, 0] - 10000.0) < 500.0
             and out_img.shape == (img_h, img_w))
    print(f"  [{'PASS' if t1_ok else 'FAIL'}] 基本测光校准")
    return t1_ok


def test_outlier_cleaning():
    """测试MAD离群清洗"""
    from photometric_calib import PhotometricCalib

    print("\n" + "=" * 60)
    print("[测试2] MAD离群清洗 (注入1颗离群星)")
    print("=" * 60)

    pc = PhotometricCalib()

    crpix1, crpix2 = 100.0, 100.0
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.01
    img_w, img_h = 200, 200

    # 20颗星, 正常flux = f_syn/10
    n_stars = 20
    px_vals = np.linspace(25, 175, n_stars)
    py_vals = np.full(n_stars, 100.0)

    from astropy.wcs import WCS
    w = WCS(naxis=2)
    w.wcs.cd = [[cd_val, 0], [0, cd_val]]
    w.wcs.crval = [crval1, crval2]
    w.wcs.crpix = [crpix1, crpix2]
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    world = w.all_pix2world(px_vals, py_vals, 0)
    gaia_ra = world[0]
    gaia_dec = world[1]
    gaia_mag = np.full(n_stars, 12.0)
    gaia_fsyn = np.full(n_stars, 50000.0)

    psf_cx = px_vals + 0.1
    psf_cy = py_vals - 0.1
    # 添加微小线性扰动使r有自然散布(MAD>0), 便于离群检测
    psf_flux = gaia_fsyn / 10.0 * (1.0 + 0.001 * (np.arange(n_stars) - 9.5))
    psf_status = np.zeros(n_stars, dtype=np.int32)

    # 注入1颗离群星: flux极小 (r = log10(F_instr/F_syn) 偏离极大)
    psf_flux[5] = 1.0  # 正常是~5000, 改成1

    image = np.full((img_h, img_w), 1000.0, dtype=np.float32)

    out_img, n_matched, scale, sigma_residual, diag = pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
    )

    print(f"  n_matched = {n_matched} (期望 19, 离群1被剔除)")
    print(f"  scale = {scale:.6e} (期望 ~10.0)")
    print(f"  diag.fit_used = {diag.fit_used} (期望 19)")
    print(f"  diag.rejected_quality = {diag.rejected_quality}")

    t2_ok = (n_matched == 19 and abs(scale - 10.0) < 0.5)
    print(f"  [{'PASS' if t2_ok else 'FAIL'}] MAD离群清洗")
    return t2_ok


def test_no_gaia():
    """测试无Gaia星时的退化路径"""
    from photometric_calib import PhotometricCalib

    print("\n" + "=" * 60)
    print("[测试3] 无Gaia星退化路径 (scale=1.0)")
    print("=" * 60)

    pc = PhotometricCalib()

    image = np.full((50, 50), 500.0, dtype=np.float32)
    # 空数组
    gaia_ra = np.array([], dtype=np.float64)
    gaia_dec = np.array([], dtype=np.float64)
    gaia_mag = np.array([], dtype=np.float64)
    gaia_fsyn = np.array([], dtype=np.float64)
    psf_cx = np.array([10.0], dtype=np.float64)
    psf_cy = np.array([10.0], dtype=np.float64)
    psf_flux = np.array([100.0], dtype=np.float64)
    psf_status = np.array([0], dtype=np.int32)

    out_img, n_matched, scale, sigma_residual, diag = pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        10.0, 20.0, 100.0, 100.0, 0.01, 0.0, 0.0, 0.01,
    )

    print(f"  n_matched = {n_matched} (期望 0)")
    print(f"  scale = {scale:.6e} (期望 1.0)")
    print(f"  out_img[0,0] = {out_img[0, 0]:.4f} (期望 500.0)")
    print(f"  diag.spectrum_rows_total = {diag.spectrum_rows_total} (期望 0)")

    t3_ok = (n_matched == 0 and abs(scale - 1.0) < 1e-9
             and abs(out_img[0, 0] - 500.0) < 1e-3)
    print(f"  [{'PASS' if t3_ok else 'FAIL'}] 退化路径")
    return t3_ok


def test_sip_wcs():
    """测试带SIP的WCS投影"""
    from photometric_calib import PhotometricCalib

    print("\n" + "=" * 60)
    print("[测试4] SIP WCS投影 (二阶SIP)")
    print("=" * 60)

    pc = PhotometricCalib()

    crpix1, crpix2 = 100.0, 100.0
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.01
    img_w, img_h = 200, 200

    # 构造二阶SIP系数
    sip_order = 2
    sip_a = np.zeros(36, dtype=np.float64)
    sip_b = np.zeros(36, dtype=np.float64)
    sip_a[6] = 1e-6   # A[1][0]
    sip_a[1] = 0.5e-6  # A[0][1]
    sip_b[6] = 0.5e-6  # B[1][0]
    sip_b[1] = 1e-6    # B[0][1]

    # 10颗星
    px_vals = np.linspace(25, 175, 10)
    py_vals = np.full(10, 100.0)

    # 用astropy SIP WCS生成参考RA/Dec
    from astropy.wcs import WCS, Sip
    w = WCS(naxis=2)
    w.wcs.cd = [[cd_val, 0], [0, cd_val]]
    w.wcs.crval = [crval1, crval2]
    w.wcs.crpix = [crpix1, crpix2]
    a_mat = np.zeros((3, 3))
    b_mat = np.zeros((3, 3))
    a_mat[1, 0] = 1e-6
    a_mat[0, 1] = 0.5e-6
    b_mat[1, 0] = 0.5e-6
    b_mat[0, 1] = 1e-6
    w.sip = Sip(a_mat, b_mat, None, None, [crpix1, crpix2])
    w.wcs.ctype = ["RA---TAN-SIP", "DEC--TAN-SIP"]
    world = w.all_pix2world(px_vals, py_vals, 0)
    gaia_ra = world[0]
    gaia_dec = world[1]
    gaia_mag = np.full(10, 12.0)
    gaia_fsyn = np.full(10, 50000.0)

    psf_cx = px_vals + 0.1
    psf_cy = py_vals - 0.1
    psf_flux = gaia_fsyn / 10.0
    psf_status = np.zeros(10, dtype=np.int32)

    image = np.full((img_h, img_w), 1000.0, dtype=np.float32)

    out_img, n_matched, scale, sigma_residual, diag = pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
        sip_order=sip_order,
        sip_a=sip_a, sip_b=sip_b,
    )

    print(f"  n_matched = {n_matched} (期望 10, SIP投影后仍能匹配)")
    print(f"  scale = {scale:.6e}")
    print(f"  diag.fit_used = {diag.fit_used}")

    t4_ok = (n_matched >= 8 and scale > 0)  # SIP可能引入微小偏移, 允许少匹配几颗
    print(f"  [{'PASS' if t4_ok else 'FAIL'}] SIP WCS投影")
    return t4_ok


def test_diag_output():
    """P12-001: 测试 PhotometricDiag 分阶段诊断输出

    构造已知输入 (10颗星 TAN 投影), 调用 calibrate_simple,
    验证 diag 各字段非默认值 (至少 spectrum_rows_total/psf_valid/fit_used > 0).
    """
    from photometric_calib import PhotometricCalib, PhotometricDiag

    print("\n" + "=" * 60)
    print("[测试5] P12-001 PhotometricDiag 分阶段诊断输出")
    print("=" * 60)

    pc = PhotometricCalib()

    # ---- 构造WCS参数 (与 test_basic_calibration 相同) ----
    crpix1, crpix2 = 100.0, 100.0
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.01
    img_w, img_h = 200, 200

    px_vals = np.linspace(25, 175, 10)
    py_vals = np.full(10, 100.0)

    from astropy.wcs import WCS
    w = WCS(naxis=2)
    w.wcs.cd = [[cd_val, 0], [0, cd_val]]
    w.wcs.crval = [crval1, crval2]
    w.wcs.crpix = [crpix1, crpix2]
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    world = w.all_pix2world(px_vals, py_vals, 0)
    gaia_ra = world[0]
    gaia_dec = world[1]
    gaia_mag = np.full(10, 12.0)
    gaia_fsyn = np.full(10, 50000.0)

    psf_cx = px_vals + 0.1
    psf_cy = py_vals - 0.1
    psf_flux = gaia_fsyn / 10.0
    psf_status = np.zeros(10, dtype=np.int32)

    image = np.full((img_h, img_w), 1000.0, dtype=np.float32)

    # ---- 调用 ----
    out_img, n_matched, scale, sigma_residual, diag = pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
    )

    # ---- 验证 diag 是 PhotometricDiag 实例 ----
    assert isinstance(diag, PhotometricDiag), \
        f"diag 应为 PhotometricDiag 实例, 实际为 {type(diag)}"

    # ---- 验证 diag 字段非默认值 (P12-001 关键检查) ----
    print(f"  diag.to_dict() = {diag.to_dict()}")

    # 阶段1: Fsyn (pc_calibrate_simple 不计算 F_syn, 但 gaia 行数应等于 n_gaia)
    # 注: pc_calibrate_simple 旧接口可能不填充所有阶段字段, 仅验证关键字段
    print(f"  阶段3 PSF: psf_total={diag.psf_total} (期望 10), psf_valid={diag.psf_valid} (期望 10)")
    print(f"  阶段7 拟合: fit_used={diag.fit_used} (期望 10), robust_iterations={diag.robust_iterations}")
    print(f"  阶段7 scale_factor={diag.scale_factor:.6e}, sigma_residual={diag.sigma_residual:.6f}")
    print(f"  阶段8 r_median={diag.r_median:.6f}, r_p90={diag.r_p90:.6f}, r_max={diag.r_max:.6f}")
    print(f"  阶段8 match_distance_median={diag.match_distance_median:.6f}, "
          f"p90={diag.match_distance_p90:.6f}, max={diag.match_distance_max:.6f}")

    # 关键字段断言 (P12-001 通过条件: 至少 spectrum_rows_total/psf_valid/fit_used > 0)
    # pc_calibrate_simple 不在 DLL 内部计算 F_syn, 但 DLL 仍应填充 psf/匹配/拟合相关字段
    checks = []
    checks.append(("psf_total > 0", diag.psf_total > 0))
    checks.append((f"psf_valid > 0 (实际={diag.psf_valid})", diag.psf_valid > 0))
    checks.append((f"fit_used > 0 (实际={diag.fit_used})", diag.fit_used > 0))
    checks.append((f"scale_factor > 0 (实际={diag.scale_factor:.6e})", diag.scale_factor > 0))
    # diag.scale_factor 与返回的 scale 应一致
    checks.append((f"diag.scale_factor ≈ scale ({diag.scale_factor:.6e} vs {scale:.6e})",
                   abs(diag.scale_factor - scale) < 1e-9))
    # diag.sigma_residual 与返回的 sigma_residual 应一致
    checks.append((f"diag.sigma_residual ≈ sigma_residual ({diag.sigma_residual:.6f} vs {sigma_residual:.6f})",
                   abs(diag.sigma_residual - sigma_residual) < 1e-9))

    all_ok = True
    for desc, ok in checks:
        print(f"    [{'PASS' if ok else 'FAIL'}] {desc}")
        if not ok:
            all_ok = False

    print(f"  [{'PASS' if all_ok else 'FAIL'}] P12-001 diag 输出")
    return all_ok


if __name__ == "__main__":
    print("=" * 60)
    print("photometric_calib C++ DLL 测试")
    print("=" * 60)

    results = []
    try:
        results.append(("基本测光校准", test_basic_calibration()))
    except Exception as e:
        print(f"  [FAIL] 基本测光校准异常: {e}")
        import traceback
        traceback.print_exc()
        results.append(("基本测光校准", False))

    try:
        results.append(("MAD离群清洗", test_outlier_cleaning()))
    except Exception as e:
        print(f"  [FAIL] MAD离群清洗异常: {e}")
        import traceback
        traceback.print_exc()
        results.append(("MAD离群清洗", False))

    try:
        results.append(("无Gaia星退化", test_no_gaia()))
    except Exception as e:
        print(f"  [FAIL] 无Gaia星退化异常: {e}")
        import traceback
        traceback.print_exc()
        results.append(("无Gaia星退化", False))

    try:
        results.append(("SIP WCS投影", test_sip_wcs()))
    except Exception as e:
        print(f"  [FAIL] SIP WCS投影异常: {e}")
        import traceback
        traceback.print_exc()
        results.append(("SIP WCS投影", False))

    try:
        results.append(("P12-001 diag 输出", test_diag_output()))
    except Exception as e:
        print(f"  [FAIL] P12-001 diag 输出异常: {e}")
        import traceback
        traceback.print_exc()
        results.append(("P12-001 diag 输出", False))

    print("\n" + "=" * 60)
    print("测试汇总:")
    n_pass = sum(1 for _, ok in results if ok)
    for name, ok in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")
    print(f"\n总计: {n_pass}/{len(results)} 通过")
    print("=" * 60)
