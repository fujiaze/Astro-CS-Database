# -*- coding: utf-8 -*-
"""
梯度估算器合成数据端到端验证
功能: 构造已知乘性/加性梯度的合成图像与匹配星表，验证 GradientEstimator.calibrate()
      能正确恢复参数并输出格式正确的结果
用途: 端到端验证梯度估算器的物理正确性 (校正后图像在星点位置恢复真实流量)，
      暴露 r 定义与校正公式之间的符号一致性问题
依赖: numpy, 同目录 estimator / wcs_transform / gradient_fitter / image_corrector
调用: python test_synthetic.py

测试设计:
  1. 合成图像 1024x1024 uint16, 30 颗均匀分布星
  2. 已知乘性梯度 M_true = 10^(0.1 + 0.15*x' + 0.10*y')  (渐晕因子)
  3. 已知加性梯度 S_true = 50 + 30*y'                    (天光背景)
  4. 图像模型: I = I_star * M_true + S_true + noise  (星点位置 I_star=5000)
  5. PSF: F_instr = I_star * M_true = 5000*M_true, B = S_true
  6. Gaia: F_syn = I_star = 5000 (无梯度理论流量)
  7. 校正目标: I_cal = (I - S) / M = I_star = 5000

物理推导 (修复后 r = log10(F_instr / F_syn)):
  r = log10(F_instr / F_syn) = log10(5000*M / 5000) = log10(M) = r_true
  M_map = 10^r = M_true
  I_cal = (I - S_map) / M_map = (5000*M + S - S) / M = 5000  ✓
  F_cal = F_instr / M_map = 5000*M / M = 5000
  scale = median(F_syn / F_cal) = 1.0
  I_final = I_cal * scale = 5000  ✓
"""

from __future__ import annotations

import csv
import os
import sys
import tempfile
import logging

import numpy as np

# 确保能导入同目录模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from estimator import GradientEstimator
from wcs_transform import WCSTransform
from gradient_fitter import GradientFitter, GradientSurface


# ============================================================================
# 合成数据生成
# ============================================================================

def build_synthetic_dataset(img_w=1024, img_h=1024, n_stars=30,
                            i_star=5000.0, noise_sigma=10.0, seed=42):
    """构造合成图像、Gaia 星表、PSF 结果

    图像模型: I(x,y) = I_star * M_true(x,y) + S_true(x,y) + noise
      - 星点位置: I_star = 5000
      - 非星点位置: I_star = 0 (背景 = S_true)
      - noise ~ N(0, sigma^2)

    乘性梯度: r_true(x,y) = 0.1 + 0.15*x' + 0.10*y'
              M_true = 10^r_true  (渐晕因子)
    加性梯度: S_true(x,y) = 50 + 30*y'

    PSF: F_instr = I_star * M_true, B = S_true
    Gaia: F_syn = I_star

    Args:
        img_w, img_h: 图像尺寸
        n_stars: 星点数 (6列x5行=30)
        i_star: 星点真实流量 (ADU)
        noise_sigma: 高斯噪声标准差
        seed: 随机种子

    Returns:
        dict: image, gaia_stars, psf_results, wcs, star_pixels, m_true_map, s_true_map
    """
    rng = np.random.default_rng(seed)

    # ---- WCS: TAN 投影, crpix=(512,512) 1-based, crval=(10,20), cd=0.002 ----
    crpix1, crpix2 = img_w / 2.0, img_h / 2.0  # 1-based 中心
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.002  # 度/像素
    wcs = WCSTransform(
        crpix1=crpix1, crpix2=crpix2,
        crval1=crval1, crval2=crval2,
        cd11=cd_val, cd12=0.0, cd21=0.0, cd22=cd_val,
    )

    # ---- 30 颗星: 6列 x 5行网格, 像素坐标在 [100, img_w-100] ----
    n_cols, n_rows = 6, 5
    assert n_cols * n_rows == n_stars
    px_vals = np.linspace(100, img_w - 100, n_cols)
    py_vals = np.linspace(100, img_h - 100, n_rows)
    px_grid, py_grid = np.meshgrid(px_vals, py_vals)
    px_flat = px_grid.ravel()  # (30,)
    py_flat = py_grid.ravel()

    # ---- 归一化坐标 x', y' ∈ [-1, 1] ----
    x_norm_stars = (2.0 * px_flat / img_w) - 1.0
    y_norm_stars = (2.0 * py_flat / img_h) - 1.0

    # ---- 已知梯度 ----
    # 乘性: r_true = 0.1 + 0.15*x' + 0.10*y', M_true = 10^r_true
    r_true_stars = 0.1 + 0.15 * x_norm_stars + 0.10 * y_norm_stars
    m_true_stars = np.power(10.0, r_true_stars)

    # 加性: S_true = 50 + 30*y'
    s_true_stars = 50.0 + 30.0 * y_norm_stars

    # ---- 全图 M_true / S_true 地图 (用于构造图像) ----
    ys = np.arange(img_h, dtype=np.float64)
    xs = np.arange(img_w, dtype=np.float64)
    x_grid_full, y_grid_full = np.meshgrid(xs, ys)  # (H, W)
    x_norm_full = (2.0 * x_grid_full / img_w) - 1.0
    y_norm_full = (2.0 * y_grid_full / img_h) - 1.0
    r_true_full = 0.1 + 0.15 * x_norm_full + 0.10 * y_norm_full
    m_true_map = np.power(10.0, r_true_full)
    s_true_map = 50.0 + 30.0 * y_norm_full

    # ---- 构造合成图像 ----
    # 非星点: I = S_true + noise
    # 星点:   I = I_star * M_true + S_true + noise
    image = s_true_map.copy()  # 背景 = S_true
    # 添加噪声
    image = image + rng.normal(0.0, noise_sigma, (img_h, img_w))

    # 在星点位置叠加星点流量 (单像素点源近似)
    star_pixels = []
    for i in range(n_stars):
        ix = int(round(px_flat[i]))
        iy = int(round(py_flat[i]))
        # 星点位置: I = I_star * M_true + S_true + noise
        # 已有 S_true + noise, 再叠加 I_star * M_true
        image[iy, ix] += i_star * m_true_stars[i]
        star_pixels.append((ix, iy))

    # 钳位到 uint16 范围
    image = np.clip(np.round(image), 0, 65535).astype(np.uint16)

    # ---- 构造 Gaia 星表 (ra/dec 由像素坐标反演) ----
    ra_arr, dec_arr = wcs.pixel_to_sky_batch(px_flat, py_flat)
    gaia_stars = []
    psf_results = []
    for i in range(n_stars):
        gaia_stars.append({
            "ra": float(ra_arr[i]),
            "dec": float(dec_arr[i]),
            "mag_g": 12.0 + 0.1 * i,
            "f_syn": float(i_star),  # 合成流量 = 真实流量 (无梯度)
            "source_id": 1000 + i,
        })
        psf_results.append({
            "status": 0,
            "B": float(s_true_stars[i]),       # PSF 局部背景 = S_true
            "flux": float(i_star * m_true_stars[i]),  # 仪器流量 = I_star * M_true
            "cx": float(px_flat[i]),
            "cy": float(py_flat[i]),
        })

    return {
        "image": image,
        "gaia_stars": gaia_stars,
        "psf_results": psf_results,
        "wcs": wcs,
        "star_pixels": star_pixels,
        "m_true_stars": m_true_stars,
        "s_true_stars": s_true_stars,
        "r_true_stars": r_true_stars,
        "m_true_map": m_true_map,
        "s_true_map": s_true_map,
        "i_star": i_star,
    }


# ============================================================================
# 辅助: 按单项式项 (j,k) 查找曲面系数
# ============================================================================

def _coeff_for(surface: GradientSurface, j: int, k: int) -> float:
    """按单项式项 (x^j * y^k) 查找曲面系数"""
    for idx, (jj, kk) in enumerate(
            GradientFitter._monomial_terms(surface.order)):
        if jj == j and kk == k:
            return float(surface.coeffs[idx])
    return float("nan")


# ============================================================================
# 测试 1: 合成数据梯度恢复 (物理正确性验证)
# ============================================================================

def test_gradient_recovery(dataset, log_dir):
    """测试梯度恢复与图像校正物理正确性

    验证:
      a. 匹配 30 颗星, 无离群排除
      b. 乘性曲面恢复 r_true = 0.1 + 0.15*x' + 0.10*y'
      c. 加性曲面恢复 S_true = 50 + 30*y'
      d. 校正后图像在星点位置接近 I_star=5000 (误差 < 10%)
      e. scale_factor 接近 1.0
    """
    print("\n" + "=" * 70)
    print("[测试 1] 合成数据梯度恢复与图像校正物理正确性")
    print("=" * 70)

    image = dataset["image"]
    gaia_stars = dataset["gaia_stars"]
    psf_results = dataset["psf_results"]
    wcs = dataset["wcs"]
    star_pixels = dataset["star_pixels"]
    i_star = dataset["i_star"]
    r_true_stars = dataset["r_true_stars"]
    s_true_stars = dataset["s_true_stars"]

    all_pass = True

    # ---- 运行 calibrate ----
    est = GradientEstimator(log_dir=log_dir, match_radius_px=3.0,
                            outlier_sigma=3.0, max_order=5)
    result = est.calibrate(image, gaia_stars, wcs, psf_results=psf_results)

    n_matched = result["n_matched"]
    n_excluded = result["n_excluded"]
    scale = result["scale_factor"]
    print(f"  n_matched={n_matched}, n_excluded={n_excluded}, scale={scale:.6e}")

    # ---- a. 匹配数量 ----
    match_ok = (n_matched == 30 and n_excluded == 0)
    print(f"  [{'PASS' if match_ok else 'FAIL'}] 30 颗全匹配, 无离群排除")
    all_pass = all_pass and match_ok

    # ---- b. 乘性曲面恢复 r_true ----
    # 物理关系: F_instr = I_star * M_true, F_syn = I_star
    #   修复后 r = log10(F_instr/F_syn) = log10(M_true) = r_true
    #   修复前 r = log10(F_syn/F_instr) = -log10(M_true) = -r_true
    mult = result["mult_surface"]
    c_const = _coeff_for(mult, 0, 0)
    c_x = _coeff_for(mult, 1, 0)
    c_y = _coeff_for(mult, 0, 1)
    print(f"\n  乘性曲面系数:")
    print(f"    常数项(0,0)={c_const:.4f} (期望 0.10)")
    print(f"    x项(1,0)={c_x:.4f} (期望 0.15)")
    print(f"    y项(0,1)={c_y:.4f} (期望 0.10)")
    # 判断是恢复 r_true 还是 -r_true
    recovers_true = (abs(c_const - 0.10) < 0.03
                     and abs(c_x - 0.15) < 0.03
                     and abs(c_y - 0.10) < 0.03)
    recovers_neg = (abs(c_const + 0.10) < 0.03
                    and abs(c_x + 0.15) < 0.03
                    and abs(c_y + 0.10) < 0.03)
    if recovers_true:
        print(f"    -> 恢复 r_true (r = log10(F_instr/F_syn), 符号正确)")
    elif recovers_neg:
        print(f"    -> 恢复 -r_true (r = log10(F_syn/F_instr), 符号相反)")
    mult_ok = recovers_true
    print(f"  [{'PASS' if mult_ok else 'FAIL'}] 乘性梯度系数恢复 r_true")
    all_pass = all_pass and mult_ok

    # ---- c. 加性曲面恢复 S_true ----
    add = result["add_surface"]
    a_const = _coeff_for(add, 0, 0)
    a_y = _coeff_for(add, 0, 1)
    a_x = _coeff_for(add, 1, 0)
    print(f"\n  加性曲面系数:")
    print(f"    常数项(0,0)={a_const:.4f} (期望 50)")
    print(f"    y项(0,1)={a_y:.4f} (期望 30)")
    print(f"    x项(1,0)={a_x:.4f} (期望 ~0)")
    add_ok = (abs(a_const - 50.0) < 5.0
              and abs(a_y - 30.0) < 5.0
              and abs(a_x) < 5.0)
    print(f"  [{'PASS' if add_ok else 'FAIL'}] 加性梯度系数恢复 S_true")
    all_pass = all_pass and add_ok

    # ---- d. 校正后图像在星点位置接近 I_star ----
    # 这是物理正确性的核心验证
    img_cal = result["image_calibrated"]
    print(f"\n  校正后图像在星点位置 (期望 ≈ {i_star:.0f}):")
    star_values = []
    for i, (ix, iy) in enumerate(star_pixels):
        val = float(img_cal[iy, ix])
        star_values.append(val)
        rel_err = abs(val - i_star) / i_star
        status = "OK" if rel_err < 0.10 else "偏离"
        print(f"    星 {i:2d} ({ix:4d},{iy:4d}): 校正={val:8.1f}, "
              f"真实={i_star:.0f}, 相对误差={rel_err*100:5.1f}% [{status}]")

    star_values = np.array(star_values)
    median_cal = float(np.median(star_values))
    mean_cal = float(np.mean(star_values))
    rel_err_median = abs(median_cal - i_star) / i_star
    rel_err_mean = abs(mean_cal - i_star) / i_star
    max_rel_err = float(np.max(np.abs(star_values - i_star) / i_star))

    print(f"\n  统计:")
    print(f"    中位数={median_cal:.1f}, 相对误差={rel_err_median*100:.1f}%")
    print(f"    均值={mean_cal:.1f}, 相对误差={rel_err_mean*100:.1f}%")
    print(f"    最大相对误差={max_rel_err*100:.1f}%")

    # 核心判定: 校正后星点流量应接近 I_star (误差 < 10%)
    recovery_ok = rel_err_median < 0.10
    print(f"  [{'PASS' if recovery_ok else 'FAIL'}] 校正后星点流量恢复 I_star "
          f"(中位数相对误差 < 10%)")
    if not recovery_ok:
        print(f"  *** 物理正确性失败: 校正后图像未恢复真实流量 ***")
        print(f"  *** 这表明 r 定义与校正公式之间存在符号不一致 ***")
        if recovers_neg:
            print(f"  *** 根因: r = log10(F_syn/F_instr) = -log10(M_true), "
                  f"M_map = 1/M_true, I_cal = (I-S)/M_map = I_star*M_true^2 ***")
    all_pass = all_pass and recovery_ok

    # ---- e. scale_factor 接近 1.0 ----
    scale_ok = abs(scale - 1.0) < 0.10
    print(f"\n  scale_factor={scale:.6e} (期望 ≈ 1.0)")
    print(f"  [{'PASS' if scale_ok else 'FAIL'}] scale_factor 接近 1.0")
    all_pass = all_pass and scale_ok

    # ---- f. 输出格式 ----
    img_cal = result["image_calibrated"]
    fmt_ok = (isinstance(img_cal, np.ndarray)
              and img_cal.dtype == np.float32
              and img_cal.shape == image.shape
              and isinstance(result["mult_surface"], GradientSurface)
              and isinstance(result["add_surface"], GradientSurface))
    print(f"\n  image_calibrated: dtype={img_cal.dtype}, shape={img_cal.shape}")
    print(f"  [{'PASS' if fmt_ok else 'FAIL'}] 输出格式正确")
    all_pass = all_pass and fmt_ok

    return all_pass, result


# ============================================================================
# 测试 2: 质量报告格式
# ============================================================================

def test_quality_report(result):
    """验证 quality_report 含全部必需字段且类型正确"""
    print("\n" + "=" * 70)
    print("[测试 2] 质量报告格式")
    print("=" * 70)

    qr = result["quality_report"]
    qr_required = {
        "n_matched", "n_excluded", "n_used",
        "mult_order", "mult_loocv_error", "mult_residual_median",
        "mult_residual_std",
        "add_order", "add_loocv_error", "add_residual_median",
        "add_residual_std", "scale_factor",
    }
    keys_ok = qr_required.issubset(qr.keys())
    type_ok = (
        isinstance(qr["n_matched"], int)
        and isinstance(qr["n_excluded"], int)
        and isinstance(qr["n_used"], int)
        and isinstance(qr["mult_order"], int)
        and isinstance(qr["mult_loocv_error"], float)
        and isinstance(qr["mult_residual_median"], float)
        and isinstance(qr["mult_residual_std"], float)
        and isinstance(qr["add_order"], int)
        and isinstance(qr["add_loocv_error"], float)
        and isinstance(qr["add_residual_median"], float)
        and isinstance(qr["add_residual_std"], float)
        and isinstance(qr["scale_factor"], float)
    )
    print(f"  必需键齐全: {keys_ok}")
    print(f"  字段类型正确: {type_ok}")
    print(f"  n_matched={qr['n_matched']}, n_excluded={qr['n_excluded']}, "
          f"n_used={qr['n_used']}")
    print(f"  mult_order={qr['mult_order']}, mult_loocv={qr['mult_loocv_error']:.6e}")
    print(f"  add_order={qr['add_order']}, add_loocv={qr['add_loocv_error']:.6e}")
    print(f"  scale_factor={qr['scale_factor']:.6e}")

    ok = keys_ok and type_ok
    print(f"  [{'PASS' if ok else 'FAIL'}] 质量报告格式")
    return ok


# ============================================================================
# 测试 3: 残差 CSV
# ============================================================================

def test_residuals_csv(log_dir, n_stars=30):
    """验证 mult_residuals.csv 和 add_residuals.csv 可写入并格式正确"""
    print("\n" + "=" * 70)
    print("[测试 3] 残差 CSV")
    print("=" * 70)

    mult_csv = os.path.join(log_dir, "mult_residuals.csv")
    add_csv = os.path.join(log_dir, "add_residuals.csv")
    csv_exist = os.path.isfile(mult_csv) and os.path.isfile(add_csv)

    if not csv_exist:
        print(f"  [FAIL] 残差 CSV 文件不存在")
        return False

    def _read_csv_rows(path):
        with open(path, "r", encoding="utf-8") as f:
            rd = csv.reader(f)
            return list(rd)

    mult_rows = _read_csv_rows(mult_csv)
    add_rows = _read_csv_rows(add_csv)

    mult_header_ok = mult_rows[0] == ["x", "y", "observed_r", "fitted_r", "weight"]
    add_header_ok = add_rows[0] == ["x", "y", "observed_b", "fitted_b", "weight"]
    mult_nrows_ok = len(mult_rows) == n_stars + 1
    add_nrows_ok = len(add_rows) == n_stars + 1

    # 验证数值可解析
    parse_ok = True
    for row in mult_rows[1:]:
        if len(row) != 5:
            parse_ok = False
            break
        try:
            float(row[0]); float(row[1]); float(row[2]); float(row[3]); float(row[4])
        except ValueError:
            parse_ok = False
            break
    if parse_ok:
        for row in add_rows[1:]:
            if len(row) != 5:
                parse_ok = False
                break
            try:
                float(row[0]); float(row[1]); float(row[2]); float(row[3]); float(row[4])
            except ValueError:
                parse_ok = False
                break

    print(f"  mult_residuals.csv: {len(mult_rows) - 1} 行, 表头正确={mult_header_ok}")
    print(f"  add_residuals.csv:  {len(add_rows) - 1} 行, 表头正确={add_header_ok}")
    print(f"  数值可解析: {parse_ok}")
    # 打印前3行示例
    print(f"  mult 示例行:")
    for r in mult_rows[1:4]:
        print(f"    {r}")
    print(f"  add 示例行:")
    for r in add_rows[1:4]:
        print(f"    {r}")

    ok = (csv_exist and mult_header_ok and add_header_ok
          and mult_nrows_ok and add_nrows_ok and parse_ok)
    print(f"  [{'PASS' if ok else 'FAIL'}] 残差 CSV 格式正确")
    return ok


# ============================================================================
# 主程序
# ============================================================================

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")

    print("=" * 70)
    print("梯度估算器合成数据端到端验证")
    print("=" * 70)

    # 临时日志/残差输出目录
    tmp_dir = tempfile.mkdtemp(prefix="gradient_synthetic_test_")
    print(f"输出目录: {tmp_dir}")

    # 构造合成数据
    print("\n构造合成数据: 1024x1024 图像, 30 颗星, 已知梯度")
    dataset = build_synthetic_dataset(
        img_w=1024, img_h=1024, n_stars=30,
        i_star=5000.0, noise_sigma=10.0, seed=42)

    img = dataset["image"]
    print(f"  图像: dtype={img.dtype}, shape={img.shape}, "
          f"范围=[{img.min()}, {img.max()}]")
    print(f"  星点数: {len(dataset['star_pixels'])}")
    print(f"  M_true 范围: [{dataset['m_true_stars'].min():.4f}, "
          f"{dataset['m_true_stars'].max():.4f}]")
    print(f"  S_true 范围: [{dataset['s_true_stars'].min():.4f}, "
          f"{dataset['s_true_stars'].max():.4f}]")
    print(f"  F_syn = {dataset['i_star']}")
    print(f"  F_instr 范围: [{(dataset['i_star']*dataset['m_true_stars']).min():.1f}, "
          f"{(dataset['i_star']*dataset['m_true_stars']).max():.1f}]")

    # 运行测试
    results = []
    t1_pass, result = test_gradient_recovery(dataset, tmp_dir)
    results.append(("测试1: 梯度恢复", t1_pass))

    t2_pass = test_quality_report(result)
    results.append(("测试2: 质量报告", t2_pass))

    t3_pass = test_residuals_csv(tmp_dir, n_stars=30)
    results.append(("测试3: 残差CSV", t3_pass))

    # 汇总
    print("\n" + "=" * 70)
    print("验证汇总")
    print("=" * 70)
    all_pass = True
    for name, ok in results:
        print(f"  {name}: {'PASS' if ok else 'FAIL'}")
        all_pass = all_pass and ok

    print(f"\n{'全部通过' if all_pass else '存在失败项'}")
    if not all_pass:
        print("\n*** 注意: 如果测试1的梯度恢复失败 (校正后图像未恢复真实流量), ***")
        print("*** 根因是 r = log10(F_syn/F_instr) 与校正公式 I_cal=(I-S)/M 不一致 ***")
        print("*** 修复: 将 r 定义改为 log10(F_instr/F_syn), 使 M_map = M_true ***")

    print("=" * 70)
    sys.exit(0 if all_pass else 1)
