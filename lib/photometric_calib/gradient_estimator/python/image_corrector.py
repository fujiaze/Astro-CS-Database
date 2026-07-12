"""
Image Corrector - 图像梯度校正与归一化
功能: 利用乘性/加性梯度曲面对天文图像进行逐像素校正, 并归一化到 Gaia 参考星系统
用途: photometric_calib 模块的校正阶段, 输出与 Gaia DR3/SP 星表系统一致的校正图像

算法依据: docs/algorithm.md 第7节
核心公式:
  1. 梯度图评估:
     - M(x,y) = 10^r(x,y), r 为乘性梯度多项式曲面 (log10(F_instr/F_syn) = log10(M_true))
     - S(x,y) = s(x,y),    s 为加性梯度多项式曲面 (PSF 背景值 B)
  2. 图像校正: I_cal(x,y) = (I(x,y) - S(x,y)) / max(M(x,y), 0.01)
  3. 通量归一化:
     - 校正后仪器流量 F_cal_i = F_instr_i / M(x_i, y_i)
     - 全局缩放 scale = median(F_syn_i / F_cal_i)
     - I_final(x,y) = I_cal(x,y) * scale

调用: from image_corrector import ImageCorrector
依赖: numpy, gradient_fitter.GradientFitter / GradientSurface
"""

from __future__ import annotations

import os
import logging
from typing import Optional, Tuple

import numpy as np

from gradient_fitter import GradientFitter, GradientSurface


# 乘性梯度下限, 防止除以过小值导致溢出 (见算法文档 7.2)
_MIN_M = 0.01


class ImageCorrector:
    """图像梯度校正与归一化器"""

    def __init__(self, log_dir: Optional[str] = None):
        self._logger = self._setup_logger(log_dir)

    @staticmethod
    def _setup_logger(log_dir: Optional[str]) -> logging.Logger:
        logger = logging.getLogger('image_corrector')
        logger.setLevel(logging.DEBUG)
        logger.propagate = False
        if logger.handlers:
            return logger
        sh = logging.StreamHandler()
        sh.setLevel(logging.INFO)
        sh.setFormatter(logging.Formatter('[%(levelname)s] %(message)s'))
        logger.addHandler(sh)
        if log_dir:
            os.makedirs(log_dir, exist_ok=True)
            fh = logging.FileHandler(
                os.path.join(log_dir, 'image_corrector.log'), encoding='utf-8')
            fh.setLevel(logging.DEBUG)
            fh.setFormatter(
                logging.Formatter('%(asctime)s [%(levelname)s] %(message)s'))
            logger.addHandler(fh)
        return logger

    # ------------------------------------------------------------------
    # 梯度图评估
    # ------------------------------------------------------------------

    def evaluate_gradient_maps(self, mult_surface: GradientSurface,
                               add_surface: GradientSurface,
                               width: int, height: int,
                               fitter: Optional[GradientFitter] = None
                               ) -> Tuple[np.ndarray, np.ndarray]:
        """评估全图像素上的乘性/加性梯度图

        参数:
            mult_surface: 乘性梯度曲面 (拟合的是 r = log10(F_instr/F_syn) = log10(M_true))
            add_surface:  加性梯度曲面 (拟合的是 s = PSF 背景值 B)
            width, height: 图像宽高 (像素)
            fitter: 梯度评估器, 为 None 时内部创建 GradientFitter()

        返回:
            (M_map, S_map): 均为 (height, width) float32 数组
            M_map = 10^(r - median_r), 围绕 1.0 (去除全局增益, 只保留空间变化)
            S_map = s - median_s, 围绕 0 (去除全局背景, 只保留空间变化)

        物理模型: I_obs = G * M(x,y) * I_star + B_0 + ΔB(x,y)
            G = 全局增益 (常数, 由 scale 吸收)
            M(x,y) = 渐晕因子 (围绕 1.0)
            B_0 = 全局背景 (常数, 不进 S_map)
            ΔB(x,y) = 背景空间变化 (围绕 0)

        r = log10(F_instr/F_syn) = log10(G) + log10(M(x,y))
            减去 median(r) 去除 log10(G), 得到 log10(M(x,y)) 的空间变化
        s = B_local = B_0 + ΔB(x,y)
            减去 median(s) 去除 B_0, 得到 ΔB(x,y)
        """
        if fitter is None:
            fitter = GradientFitter()

        # 乘性梯度: r = log10(F_instr/F_syn) = log10(G) + log10(M_true)
        # 减去中位数去除全局增益 log10(G), M_map 围绕 1.0
        r_map = fitter.evaluate_surface_fullimage(mult_surface, width, height)
        r_offset = float(np.median(r_map))
        r_map = r_map - r_offset
        M_map = np.power(10.0, r_map).astype(np.float32)
        M_map = np.maximum(M_map, _MIN_M)

        # 加性梯度: s = B_0 + ΔB(x,y)
        # 减去中位数去除全局背景 B_0, S_map 围绕 0
        S_map_raw = fitter.evaluate_surface_fullimage(
            add_surface, width, height)
        s_offset = float(np.median(S_map_raw))
        S_map = (S_map_raw - s_offset).astype(np.float32)

        self._logger.info(
            f"梯度图评估完成: 尺寸={width}x{height}, "
            f"r_offset={r_offset:.6f} (全局增益 log10(G)), "
            f"s_offset={s_offset:.2f} (全局背景 B_0), "
            f"M_map 范围=[{float(M_map.min()):.4f}, {float(M_map.max()):.4f}], "
            f"S_map 范围=[{float(S_map.min()):.4f}, {float(S_map.max()):.4f}]")
        return M_map, S_map

    # ------------------------------------------------------------------
    # 图像校正
    # ------------------------------------------------------------------

    def correct(self, image: np.ndarray,
                mult_surface: GradientSurface,
                add_surface: GradientSurface,
                fitter: Optional[GradientFitter] = None) -> np.ndarray:
        """校正图像: I_cal = (I - S) / max(M, 0.01)

        参数:
            image: 原图像 uint16 或 float, 形状 (height, width)
            mult_surface / add_surface: 乘性/加性梯度曲面
            fitter: 梯度评估器, 为 None 时内部创建

        返回:
            校正后 float32 图像 (height, width)
        """
        image = np.asarray(image)
        if image.ndim != 2:
            raise ValueError(f"输入图像必须为2D, 实际为 {image.ndim}D")

        height, width = image.shape
        M_map, S_map = self.evaluate_gradient_maps(
            mult_surface, add_surface, width, height, fitter)

        I_float = image.astype(np.float32)
        # M_map 已钳位至 >= 0.01, 此处再次取 max 保证数值安全
        I_cal = (I_float - S_map) / np.maximum(M_map, _MIN_M)

        self._logger.info(
            f"图像校正完成: 尺寸={width}x{height}, "
            f"校正后范围=[{float(I_cal.min()):.4f}, {float(I_cal.max()):.4f}]")
        return I_cal.astype(np.float32)

    # ------------------------------------------------------------------
    # 通量归一化
    # ------------------------------------------------------------------

    def normalize(self, image_cal: np.ndarray,
                  matches_x: np.ndarray, matches_y: np.ndarray,
                  f_syn_arr: np.ndarray, f_instr_arr: np.ndarray,
                  mult_surface: GradientSurface,
                  fitter: Optional[GradientFitter] = None
                  ) -> Tuple[np.ndarray, float]:
        """归一化校正图像到 Gaia 参考星系统

        参数:
            image_cal: 校正后图像 (height, width) float32
            matches_x, matches_y: 匹配星像素坐标 (0-based)
            f_syn_arr: 合成流量数组 (Gaia 参考系统)
            f_instr_arr: 仪器流量数组 (dynamic_psf 的 flux 字段)
            mult_surface: 乘性梯度曲面
            fitter: 梯度评估器, 为 None 时内部创建

        计算:
            F_cal_i = F_instr_i / M(x_i, y_i)
            scale = median(F_syn_i / F_cal_i)
            I_final = image_cal * scale

        返回:
            (I_final, scale), I_final 为 float32
        """
        if fitter is None:
            fitter = GradientFitter()

        image_cal = np.asarray(image_cal, dtype=np.float32)
        matches_x = np.asarray(matches_x, dtype=np.float64)
        matches_y = np.asarray(matches_y, dtype=np.float64)
        f_syn_arr = np.asarray(f_syn_arr, dtype=np.float64)
        f_instr_arr = np.asarray(f_instr_arr, dtype=np.float64)

        height, width = image_cal.shape
        n = matches_x.shape[0]

        if n == 0:
            self._logger.warning("匹配星数为0, scale 退化为 1.0")
            return image_cal.copy(), 1.0

        # 坐标归一化到 [-1,1], 与曲面拟合一致
        x_norm = (2.0 * matches_x / width) - 1.0
        y_norm = (2.0 * matches_y / height) - 1.0

        # 评估匹配星位置的乘性梯度 r(x_i, y_i), M = 10^r
        r_vals = fitter.evaluate_surface(mult_surface, x_norm, y_norm)
        M_vals = np.power(10.0, r_vals)
        M_vals = np.maximum(M_vals, _MIN_M)

        # 有效掩码: 仪器流量与合成流量均 > 0, 且 M 有限
        valid = (f_instr_arr > 0.0) & (f_syn_arr > 0.0) & np.isfinite(M_vals)
        n_valid = int(np.sum(valid))
        if n_valid == 0:
            self._logger.warning("无有效匹配星 (流量<=0 或 M 非法), scale 退化为 1.0")
            return image_cal.copy(), 1.0

        # 校正后仪器流量 F_cal = F_instr / M
        F_cal = f_instr_arr[valid] / M_vals[valid]
        # 全局缩放: scale = median(F_syn / F_cal)
        ratios = f_syn_arr[valid] / F_cal
        scale = float(np.median(ratios))

        I_final = (image_cal * scale).astype(np.float32)

        self._logger.info(
            f"归一化完成: 匹配星数={n}, 有效星数={n_valid}, "
            f"scale={scale:.6e}, "
            f"M范围=[{float(M_vals[valid].min()):.4f}, "
            f"{float(M_vals[valid].max()):.4f}]")
        return I_final, scale

    # ------------------------------------------------------------------
    # 一站式: 校正 + 归一化
    # ------------------------------------------------------------------

    def correct_and_normalize(self, image: np.ndarray,
                              mult_surface: GradientSurface,
                              add_surface: GradientSurface,
                              matches_x: np.ndarray, matches_y: np.ndarray,
                              f_syn_arr: np.ndarray, f_instr_arr: np.ndarray,
                              fitter: Optional[GradientFitter] = None
                              ) -> Tuple[np.ndarray, float, np.ndarray, np.ndarray]:
        """一站式: 图像校正 + 通量归一化

        返回:
            (I_final, scale, M_map, S_map)
            I_final: 归一化后 float32 图像 (height, width)
            scale: 全局缩放因子
            M_map: 乘性梯度图 (height, width) float32
            S_map: 加性梯度图 (height, width) float32
        """
        if fitter is None:
            fitter = GradientFitter()

        image = np.asarray(image)
        if image.ndim != 2:
            raise ValueError(f"输入图像必须为2D, 实际为 {image.ndim}D")
        height, width = image.shape

        # 评估梯度图 (仅计算一次)
        M_map, S_map = self.evaluate_gradient_maps(
            mult_surface, add_surface, width, height, fitter)

        # 图像校正
        I_float = image.astype(np.float32)
        I_cal = (I_float - S_map) / np.maximum(M_map, _MIN_M)

        # 通量归一化
        I_final, scale = self.normalize(
            I_cal, matches_x, matches_y, f_syn_arr, f_instr_arr,
            mult_surface, fitter)

        self._logger.info(
            f"一站式校正+归一化完成: 尺寸={width}x{height}, scale={scale:.6e}")
        return I_final, scale, M_map, S_map


# ======================================================================
# 合成数据测试
# ======================================================================
if __name__ == '__main__':
    np.random.seed(42)

    # 测试图像: 100x100 uint16, 值 1000
    width, height = 100, 100
    image = np.full((height, width), 1000, dtype=np.uint16)

    # 1阶乘性曲面: coeffs=[0.1, 0, 0] -> r=0.1, M=10^0.1
    # 单项式基 (order=1): [(0,0), (1,0), (0,1)] = [常数, x, y]
    mult_surface = GradientSurface(
        order=1, coeffs=np.array([0.1, 0.0, 0.0]),
        loocv_error=0.0, residual_median=0.0, residual_std=0.0,
        n_used=100, n_rejected=0)

    # 1阶加性曲面: coeffs=[100, 0, 0] -> s=100, S=100
    add_surface = GradientSurface(
        order=1, coeffs=np.array([100.0, 0.0, 0.0]),
        loocv_error=0.0, residual_median=0.0, residual_std=0.0,
        n_used=100, n_rejected=0)

    log_dir = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'logs')
    corrector = ImageCorrector(log_dir=log_dir)

    expected_M = 10.0 ** 0.1
    expected_I_cal = (1000.0 - 100.0) / expected_M

    print("=" * 60)
    print("ImageCorrector 合成数据验证")
    print("=" * 60)
    print(f"测试图像: {width}x{width} uint16, 值=1000")
    print(f"乘性曲面: r=0.1 -> M=10^0.1={expected_M:.6f}")
    print(f"加性曲面: S=100")
    print(f"期望校正值: (1000-100)/{expected_M:.6f} = {expected_I_cal:.4f}")

    # --- 测试1: evaluate_gradient_maps ---
    print("\n[测试1] evaluate_gradient_maps")
    M_map, S_map = corrector.evaluate_gradient_maps(
        mult_surface, add_surface, width, height)
    m_min = float(M_map.min())
    m_mean = float(M_map.mean())
    s_mean = float(S_map.mean())
    print(f"  M_map: shape={M_map.shape}, dtype={M_map.dtype}, "
          f"min={m_min:.6f}, mean={m_mean:.6f}")
    print(f"  S_map: shape={S_map.shape}, dtype={S_map.dtype}, "
          f"mean={s_mean:.6f}")
    t1_pass = (M_map.shape == (height, width)
               and S_map.shape == (height, width)
               and M_map.dtype == np.float32
               and S_map.dtype == np.float32
               and abs(m_mean - expected_M) < 1e-4
               and abs(s_mean - 100.0) < 1e-3
               and m_min >= 0.01)
    print(f"  -> M_map.min() >= 0.01: {'通过' if m_min >= 0.01 else '失败'}")
    print(f"  -> 结果: {'通过' if t1_pass else '失败'}")

    # --- 测试2: correct ---
    print("\n[测试2] correct")
    I_cal = corrector.correct(image, mult_surface, add_surface)
    cal_mean = float(I_cal.mean())
    cal_std = float(I_cal.std())
    print(f"  I_cal: shape={I_cal.shape}, dtype={I_cal.dtype}, "
          f"mean={cal_mean:.4f}, std={cal_std:.6f}")
    t2_pass = (I_cal.dtype == np.float32
               and abs(cal_mean - expected_I_cal) < 1e-2
               and cal_std < 1e-3)
    print(f"  -> 期望均值={expected_I_cal:.4f}, 实际={cal_mean:.4f}, "
          f"误差={abs(cal_mean - expected_I_cal):.6f}")
    print(f"  -> 结果: {'通过' if t2_pass else '失败'}")

    # --- 测试3: normalize ---
    # 物理一致性: 图像模型 I = I_star*M + S
    #   I_star = (I - S)/M = 714.902 (校正值)
    #   F_instr = I_star * M = 714.902 * 1.2589 = 900 (PSF flux, 含乘性梯度)
    #   F_syn   = I_star = 714.902 (合成流量 = 真实星顶流量)
    #   F_cal   = F_instr / M = 900 / 1.2589 = 714.902
    #   scale   = median(F_syn / F_cal) = 714.902/714.902 = 1.0
    print("\n[测试3] normalize")
    f_instr = np.array([900.0, 900.0, 900.0])
    f_syn = np.array([expected_I_cal, expected_I_cal, expected_I_cal])
    matches_x = np.array([25.0, 50.0, 75.0])
    matches_y = np.array([25.0, 50.0, 75.0])
    I_final, scale = corrector.normalize(
        I_cal, matches_x, matches_y, f_syn, f_instr, mult_surface)
    print(f"  scale={scale:.6f} (期望 1.0)")
    print(f"  I_final: mean={float(I_final.mean()):.4f} (期望 {expected_I_cal:.4f})")
    t3_pass = abs(scale - 1.0) < 1e-4
    print(f"  -> 结果: {'通过' if t3_pass else '失败'}")

    # --- 测试4: correct_and_normalize ---
    print("\n[测试4] correct_and_normalize")
    I_final2, scale2, M_map2, S_map2 = corrector.correct_and_normalize(
        image, mult_surface, add_surface,
        matches_x, matches_y, f_syn, f_instr)
    print(f"  scale={scale2:.6f}")
    print(f"  I_final2: mean={float(I_final2.mean()):.4f}")
    print(f"  M_map2.min()={float(M_map2.min()):.6f}")
    t4_pass = (abs(scale2 - 1.0) < 1e-4
               and M_map2.shape == (height, width)
               and S_map2.shape == (height, width)
               and float(M_map2.min()) >= 0.01)
    print(f"  -> 结果: {'通过' if t4_pass else '失败'}")

    # --- 测试5: M_map 下限钳位 ---
    print("\n[测试5] M_map 下限钳位 (r=-10 -> M 应钳位为 0.01)")
    extreme_surface = GradientSurface(
        order=1, coeffs=np.array([-10.0, 0.0, 0.0]),
        loocv_error=0.0, residual_median=0.0, residual_std=0.0,
        n_used=1, n_rejected=0)
    M_ext, _ = corrector.evaluate_gradient_maps(
        extreme_surface, add_surface, width, height)
    t5_pass = abs(float(M_ext.min()) - 0.01) < 1e-6
    print(f"  M_ext.min()={float(M_ext.min()):.6f} (期望 0.01)")
    print(f"  -> 结果: {'通过' if t5_pass else '失败'}")

    all_pass = all([t1_pass, t2_pass, t3_pass, t4_pass, t5_pass])
    print("\n" + "=" * 60)
    print(f"测试结果: {'全部通过' if all_pass else '存在失败项'}")
    print("=" * 60)
