# -*- coding: utf-8 -*-
"""
Gradient Estimator - 梯度估算器主程序
功能: 串联星检测/PSF拟合、星-图匹配、乘性/加性梯度曲面拟合、图像校正与归一化，
      完成天文图像的光度定标全流程，输出与 Gaia 参考星系统一致的校正图像
用途: photometric_calib 模块的主入口；调用 star_matcher / gradient_fitter /
      image_corrector 完成端到端定标；可选内置 star_detector + dynamic_psf
依赖: numpy; 同目录 star_matcher / gradient_fitter / image_corrector;
      内置 PSF 路径额外依赖 star_detector / dynamic_psf (惰性导入, 不影响模块 import)
调用: from estimator import GradientEstimator
      est = GradientEstimator(log_dir=None)
      result = est.calibrate(image, gaia_stars, wcs_transform, psf_results=None)

算法流程 (calibrate):
  1. psf_results 为 None 时, 内部调用星检测 + PSF 拟合
  2. StarMatcher.match_and_clean: Gaia 星 <-> PSF 星空间匹配 + MAD 离群清洗
  3. 匹配星数 < 6 时返回退化结果 (不拟合梯度)
  4. to_arrays 取数组; 乘性输入 r = log10(F_instr / F_syn)
  5. GradientFitter.fit_multiplicative / fit_additive 拟合乘性/加性曲面
  6. ImageCorrector.correct_and_normalize 校正+归一化 -> (I_final, scale, M_map, S_map)
  7. 构建质量报告, 保存残差 CSV
"""

from __future__ import annotations

import csv
import logging
import os
from typing import Optional

import numpy as np

from star_matcher import StarMatcher, StarMatch
from gradient_fitter import GradientFitter, GradientSurface
from image_corrector import ImageCorrector


logger = logging.getLogger(__name__)

# 退化阈值: 匹配星数少于此值时不拟合梯度, 返回恒等曲面
_MIN_MATCHED_FOR_FIT = 6

# 空间信号检测阈值: R²低于此值时认为无可检测梯度, 跳过校正
# R² = 1 - SS_res/SS_tot, 在IRLS内点(weight>0)上计算
# R² < 0.02 表示拟合解释的方差不到2%, 梯度信号不可区分于噪声
_MIN_R_SQUARED = 0.02
_SNR_GRID_N = 5  # SNR诊断用的网格大小


def _identity_surface() -> GradientSurface:
    """构造恒等梯度曲面 (1阶, 系数全0): 乘性 r=0 -> M=1; 加性 s=0 -> S=0"""
    return GradientSurface(
        order=1,
        coeffs=np.zeros(3, dtype=np.float64),
        loocv_error=float("inf"),
        residual_median=0.0,
        residual_std=0.0,
        n_used=0,
        n_rejected=0,
    )


class GradientEstimator:
    """梯度估算器: 光度定标全流程主程序

    串联 StarMatcher -> GradientFitter -> ImageCorrector, 完成从原图到
    Gaia 参考系统归一化图像的端到端定标。
    """

    def __init__(self, log_dir: Optional[str] = None,
                 match_radius_px: float = 3.0,
                 outlier_sigma: float = 3.0,
                 max_order: int = 5):
        """初始化梯度估算器

        Args:
            log_dir: 日志目录, None 仅输出到控制台; 同时作为残差 CSV 输出目录
            match_radius_px: 星-图匹配半径 (像素)
            outlier_sigma: 离群点清洗 sigma 阈值
            max_order: 梯度曲面最高阶数
        """
        self._log_dir = log_dir
        self._match_radius_px = float(match_radius_px)
        self._outlier_sigma = float(outlier_sigma)
        self._max_order = int(max_order)
        self._logger = self._setup_logger(log_dir)

        # calibrate 期间填充的状态 (供 _build_quality_report / _save_residuals_csv 使用)
        self._img_w: int = 0
        self._img_h: int = 0
        self._n_excluded: int = 0
        self._mult_r2: float = 0.0
        self._add_r2: float = 0.0

        self._logger.info(
            "GradientEstimator 初始化: match_radius=%.2f px, outlier_sigma=%.2f, max_order=%d",
            self._match_radius_px, self._outlier_sigma, self._max_order)

    # ------------------------------------------------------------------
    # 日志
    # ------------------------------------------------------------------

    @staticmethod
    def _setup_logger(log_dir: Optional[str]) -> logging.Logger:
        lg = logging.getLogger("gradient_estimator")
        lg.setLevel(logging.DEBUG)
        lg.propagate = False
        if lg.handlers:
            return lg
        sh = logging.StreamHandler()
        sh.setLevel(logging.INFO)
        sh.setFormatter(logging.Formatter("[%(levelname)s] %(message)s"))
        lg.addHandler(sh)
        if log_dir:
            os.makedirs(log_dir, exist_ok=True)
            fh = logging.FileHandler(
                os.path.join(log_dir, "estimator.log"), encoding="utf-8")
            fh.setLevel(logging.DEBUG)
            fh.setFormatter(
                logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
            lg.addHandler(fh)
        return lg

    # ------------------------------------------------------------------
    # 信号检测: R² 计算
    # ------------------------------------------------------------------

    @staticmethod
    def _compute_r_squared(fitter: GradientFitter, surface: GradientSurface,
                           x_arr: np.ndarray, y_arr: np.ndarray,
                           val_arr: np.ndarray, img_w: float,
                           img_h: float) -> float:
        """计算拟合的 R² (决定系数), 仅在 IRLS 内点 (weight>0) 上计算

        R² = 1 - SS_res / SS_tot
        - SS_res = Σ(val_i - fit_i)²  (内点残差平方和)
        - SS_tot = Σ(val_i - mean_val)²  (内点总平方和)

        R² < 0 表示拟合比均值还差; R² ≈ 0 表示无空间信号; R² → 1 表示完美拟合

        Returns:
            R² 标量, 若内点不足则返回 0.0
        """
        x_norm = (2.0 * x_arr / img_w) - 1.0
        y_norm = (2.0 * y_arr / img_h) - 1.0
        X = fitter._build_design_matrix(x_norm, y_norm, surface.order)
        _, weights, _ = fitter._irls_fit(X, val_arr)

        inlier = weights > 0.0
        n_in = int(np.sum(inlier))
        if n_in < 3:
            return 0.0

        val_in = val_arr[inlier]
        fit_in = (X @ surface.coeffs)[inlier]
        ss_res = float(np.sum((val_in - fit_in) ** 2))
        ss_tot = float(np.sum((val_in - np.mean(val_in)) ** 2))
        if ss_tot < 1e-15:
            return 0.0
        return 1.0 - ss_res / ss_tot

    # ------------------------------------------------------------------
    # 完整校准流程
    # ------------------------------------------------------------------

    def calibrate(self, image, gaia_stars, wcs_transform,
                  psf_results=None) -> dict:
        """完整光度定标流程

        Args:
            image: 原图 numpy 2D 数组 (uint16 或 float)
            gaia_stars: list[dict], 每项含 ra, dec, mag_g, f_syn, source_id
            wcs_transform: WCSTransform 对象
            psf_results: 可选, list[DPSFFitResultPy]; None 时内部调用星检测+PSF拟合

        Returns:
            dict: {
                "image_calibrated": np.ndarray float32,
                "mult_surface": GradientSurface,
                "add_surface": GradientSurface,
                "scale_factor": float,
                "n_matched": int,
                "n_excluded": int,
                "quality_report": dict,
            }
        """
        image = np.asarray(image)
        if image.ndim != 2:
            raise ValueError(f"输入图像必须为2D, 实际为 {image.ndim}D")
        img_h, img_w = image.shape
        self._img_w = img_w
        self._img_h = img_h
        self._logger.info(
            "calibrate 开始: 图像尺寸=%dx%d, Gaia 星=%d 颗, PSF 结果=%s",
            img_w, img_h, len(gaia_stars),
            "None(将内部检测)" if psf_results is None else f"{len(psf_results)} 颗")

        # 1. PSF 结果: 为 None 时内部调用星检测 + PSF 拟合
        if psf_results is None:
            self._logger.info("未提供 psf_results, 内部调用星检测 + PSF 拟合")
            psf_results = self._detect_and_fit_psf(image)
            self._logger.info("内部 PSF 拟合完成: %d 颗", len(psf_results))

        # 2. 星-图匹配 + 离群清洗
        matcher = StarMatcher(log_dir=self._log_dir)
        matches, n_excluded = matcher.match_and_clean(
            wcs_transform, gaia_stars, psf_results,
            match_radius_px=self._match_radius_px,
            outlier_sigma=self._outlier_sigma)
        self._n_excluded = int(n_excluded)
        n_matched = len(matches)
        self._logger.info(
            "星-图匹配完成: 匹配 %d 对, 排除 %d 对", n_matched, n_excluded)

        # 3. 匹配星数不足, 返回退化结果
        if n_matched < _MIN_MATCHED_FOR_FIT:
            self._logger.warning(
                "匹配星数 %d < %d, 样本不足无法拟合梯度, 返回退化结果 (恒等曲面, scale=1.0)",
                n_matched, _MIN_MATCHED_FOR_FIT)
            identity = _identity_surface()
            return {
                "image_calibrated": image.astype(np.float32),
                "mult_surface": identity,
                "add_surface": identity,
                "scale_factor": 1.0,
                "n_matched": n_matched,
                "n_excluded": self._n_excluded,
                "quality_report": self._build_quality_report(
                    matches, identity, identity, 1.0),
            }

        # 4. 转数组
        arrays = matcher.to_arrays(matches)
        x_arr = arrays["x"]
        y_arr = arrays["y"]
        f_instr = arrays["f_instr"]
        f_syn = arrays["f_syn"]
        b_local = arrays["b_local"]

        # 5. 乘性梯度输入: r = log10(F_instr / F_syn)
        #    (clean_outliers 已保证 F_instr>0, F_syn>0)
        #    r = log10(M_true), M_map = 10^r = M_true (渐晕因子)
        #    校正 I_cal = (I - S) / M_map = I_star (物理一致)
        valid = (f_instr > 0.0) & (f_syn > 0.0)
        if not np.all(valid):
            self._logger.warning(
                "匹配星中存在 F_instr/F_syn <= 0 的无效项 %d 个, 已剔除后再拟合",
                int((~valid).sum()))
            x_arr = x_arr[valid]
            y_arr = y_arr[valid]
            f_instr = f_instr[valid]
            f_syn = f_syn[valid]
            b_local = b_local[valid]
        r_arr = np.log10(f_instr / f_syn)

        # 6. 乘性梯度曲面拟合
        fitter = GradientFitter(log_dir=self._log_dir)
        mult_surface = fitter.fit_multiplicative(
            x_arr, y_arr, r_arr, img_w, img_h, max_order=self._max_order)
        self._logger.info(
            "乘性梯度拟合: 阶数=%d, LOOCV=%.6e, 使用=%d, 排除=%d",
            mult_surface.order, mult_surface.loocv_error,
            mult_surface.n_used, mult_surface.n_rejected)

        # 6.5 乘性信号检测: R² < 阈值则跳过乘性校正, 避免人为引入梯度
        mult_r2 = self._compute_r_squared(
            fitter, mult_surface, x_arr, y_arr, r_arr, img_w, img_h)
        self._mult_r2 = float(mult_r2)
        self._logger.info(
            "乘性信号检测: R²=%.4f, 阈值=%.2f", mult_r2, _MIN_R_SQUARED)
        if mult_r2 < _MIN_R_SQUARED:
            self._logger.warning(
                "乘性梯度 R²=%.4f < 阈值%.2f, 拟合主要在拟合噪声, "
                "跳过乘性校正 (M_map=1.0)。可能原因: 拥挤场PSF测光噪声大/"
                "图像已做平场/渐晕太弱。",
                mult_r2, _MIN_R_SQUARED)
            mult_surface = _identity_surface()

        # 7. 加性梯度曲面拟合 (PSF 背景值 B)
        # [封存 2026-07-12] 天光校正已封存，只保留乘性流量定标
        # 原因: S_map 经验多项式拟合无法区分缓变天光与缓变星云信号，
        #       在银河等区域会误减目标信号。天光一致性留给后续马赛克背景匹配模块。
        # 恢复方法: 取消下方注释即可
        # add_surface = fitter.fit_additive(
        #     x_arr, y_arr, b_local, img_w, img_h, max_order=self._max_order)
        # self._logger.info(
        #     "加性梯度拟合: 阶数=%d, LOOCV=%.6e, 使用=%d, 排除=%d",
        #     add_surface.order, add_surface.loocv_error,
        #     add_surface.n_used, add_surface.n_rejected)
        add_surface = _identity_surface()
        self._logger.info("天光校正已封存 (add_surface=identity, S_map=0), 仅做乘性流量定标")

        # 7.5 加性信号检测: R² < 阈值则跳过加性校正
        # [封存 2026-07-12] 随加性梯度拟合一同封存
        # add_r2 = self._compute_r_squared(
        #     fitter, add_surface, x_arr, y_arr, b_local, img_w, img_h)
        # self._add_r2 = float(add_r2)
        # self._logger.info(
        #     "加性信号检测: R²=%.4f, 阈值=%.2f", add_r2, _MIN_R_SQUARED)
        # if add_r2 < _MIN_R_SQUARED:
        #     self._logger.warning(
        #         "加性梯度 R²=%.4f < 阈值%.2f, 跳过加性校正 (S_map=0.0)。",
        #         add_r2, _MIN_R_SQUARED)
        #     add_surface = _identity_surface()
        self._add_r2 = 0.0

        # 8. 图像校正 + 通量归一化
        corrector = ImageCorrector(log_dir=self._log_dir)
        I_final, scale, M_map, S_map = corrector.correct_and_normalize(
            image, mult_surface, add_surface,
            x_arr, y_arr, f_syn, f_instr, fitter=fitter)
        self._logger.info(
            "校正+归一化完成: scale=%.6e, I_final dtype=%s, 范围=[%.4f, %.4f]",
            scale, I_final.dtype, float(I_final.min()), float(I_final.max()))

        # 9. 质量报告
        quality_report = self._build_quality_report(
            matches, mult_surface, add_surface, scale)

        # 10. 保存残差 CSV (诊断用)
        if self._log_dir:
            try:
                self._save_residuals_csv(
                    matches, mult_surface, add_surface, fitter, self._log_dir)
            except Exception as e:
                self._logger.warning("残差 CSV 保存失败: %s", e)

        self._logger.info(
            "calibrate 完成: n_matched=%d, n_excluded=%d, scale=%.6e",
            n_matched, self._n_excluded, scale)

        return {
            "image_calibrated": I_final,
            "mult_surface": mult_surface,
            "add_surface": add_surface,
            "scale_factor": float(scale),
            "n_matched": n_matched,
            "n_excluded": self._n_excluded,
            "quality_report": quality_report,
        }

    # ------------------------------------------------------------------
    # 内置星检测 + PSF 拟合
    # ------------------------------------------------------------------

    def _detect_and_fit_psf(self, image) -> list:
        """内置星检测 + PSF 拟合 (惰性导入, 不影响模块 import)

        Args:
            image: 原图 2D 数组

        Returns:
            list[DPSFFitResultPy], 每项含 status, B, A, cx, cy, flux 等
        """
        import sys

        # star_detector: lib/star_detector/python
        sd_path = os.path.normpath(os.path.join(
            os.path.dirname(__file__), "..", "..", "..",
            "star_detector", "python"))
        if sd_path not in sys.path:
            sys.path.insert(0, sd_path)
        from star_detector import StarDetector

        # dynamic_psf: lib/dynamic_psf/python
        dpsf_path = os.path.normpath(os.path.join(
            os.path.dirname(__file__), "..", "..", "..",
            "dynamic_psf", "python"))
        if dpsf_path not in sys.path:
            sys.path.insert(0, dpsf_path)
        from dynamic_psf import DynamicPSF

        # 1. 星检测
        self._logger.info("内置星检测开始")
        detector = StarDetector()
        coords = detector.detect(image)
        self._logger.info("星检测完成: %d 颗", len(coords))

        # 2. PSF 批量拟合
        cx_list = [float(c[0]) for c in coords]
        cy_list = [float(c[1]) for c in coords]
        results = DynamicPSF.fit_batch(image, cx_list, cy_list)
        n_ok = sum(1 for r in results if int(r.status) == 0)
        self._logger.info(
            "PSF 拟合完成: %d 颗, 成功 %d 颗", len(results), n_ok)
        return results

    # ------------------------------------------------------------------
    # 质量报告
    # ------------------------------------------------------------------

    def _build_quality_report(self, matches, mult_surface,
                              add_surface, scale) -> dict:
        """构建质量报告

        Args:
            matches: StarMatch 列表 (清洗后)
            mult_surface: 乘性梯度曲面
            add_surface: 加性梯度曲面
            scale: 全局缩放因子

        Returns:
            质量报告 dict
        """
        return {
            "n_matched": len(matches),
            "n_excluded": int(self._n_excluded),
            "n_used": int(mult_surface.n_used),
            "mult_order": int(mult_surface.order),
            "mult_loocv_error": float(mult_surface.loocv_error),
            "mult_residual_median": float(mult_surface.residual_median),
            "mult_residual_std": float(mult_surface.residual_std),
            "mult_r_squared": float(self._mult_r2),
            "mult_skipped": self._mult_r2 < _MIN_R_SQUARED,
            "add_order": int(add_surface.order),
            "add_loocv_error": float(add_surface.loocv_error),
            "add_residual_median": float(add_surface.residual_median),
            "add_residual_std": float(add_surface.residual_std),
            "add_r_squared": float(self._add_r2),
            "add_skipped": self._add_r2 < _MIN_R_SQUARED,
            "scale_factor": float(scale),
            # [封存 2026-07-12] 天空校正已封存，仅做乘性流量定标
            "sky_calibration_frozen": True,
        }

    # ------------------------------------------------------------------
    # 残差 CSV 保存
    # ------------------------------------------------------------------

    def _save_residuals_csv(self, matches, mult_surface, add_surface,
                            fitter, output_dir):
        """保存乘性/加性残差 CSV (诊断用)

        mult_residuals.csv: x, y, observed_r, fitted_r, weight
            observed_r = log10(F_instr / F_syn) = log10(M_true)
            fitted_r   = 多项式曲面评估值
            weight     = IRLS (Tukey biweight) 权重
        add_residuals.csv:  x, y, observed_b, fitted_b, weight
            observed_b = PSF 局部背景 B
            fitted_b   = 多项式曲面评估值
            weight     = IRLS 权重

        Args:
            matches: StarMatch 列表
            mult_surface / add_surface: 拟合曲面
            fitter: GradientFitter (复用其设计矩阵/IRLS 计算, 保证权重一致)
            output_dir: 输出目录
        """
        os.makedirs(output_dir, exist_ok=True)
        n = len(matches)
        if n == 0:
            self._logger.warning("无匹配星, 跳过残差 CSV 保存")
            return

        x = np.array([m.x for m in matches], dtype=np.float64)
        y = np.array([m.y for m in matches], dtype=np.float64)
        f_syn = np.array([m.f_syn for m in matches], dtype=np.float64)
        f_instr = np.array([m.f_instr for m in matches], dtype=np.float64)
        b_local = np.array([m.b_local for m in matches], dtype=np.float64)

        # 坐标归一化到 [-1,1], 与 GradientFitter 一致
        img_w = float(self._img_w)
        img_h = float(self._img_h)
        x_norm = (2.0 * x / img_w) - 1.0
        y_norm = (2.0 * y / img_h) - 1.0

        # ---- 乘性残差 ----
        r_obs = np.log10(f_instr / f_syn)
        X_mult = fitter._build_design_matrix(x_norm, y_norm, mult_surface.order)
        r_fit = X_mult @ mult_surface.coeffs
        # 复用 IRLS 拟合获取 Tukey biweight 权重 (与拟合阶段一致)
        _, w_mult, _ = fitter._irls_fit(X_mult, r_obs)

        mult_path = os.path.join(output_dir, "mult_residuals.csv")
        with open(mult_path, "w", encoding="utf-8", newline="") as f:
            wr = csv.writer(f)
            wr.writerow(["x", "y", "observed_r", "fitted_r", "weight"])
            for i in range(n):
                wr.writerow([
                    f"{x[i]:.4f}", f"{y[i]:.4f}",
                    f"{r_obs[i]:.6f}", f"{r_fit[i]:.6f}",
                    f"{w_mult[i]:.4f}",
                ])
        self._logger.info("乘性残差已保存: %s (%d 行)", mult_path, n)

        # ---- 加性残差 ----
        # [封存 2026-07-12] 天空校正已封存，跳过加性残差输出 (add_surface=identity)
        # 恢复方法: 取消下方注释即可
        # b_obs = b_local
        # X_add = fitter._build_design_matrix(x_norm, y_norm, add_surface.order)
        # b_fit = X_add @ add_surface.coeffs
        # _, w_add, _ = fitter._irls_fit(X_add, b_obs)
        #
        # add_path = os.path.join(output_dir, "add_residuals.csv")
        # with open(add_path, "w", encoding="utf-8", newline="") as f:
        #     wr = csv.writer(f)
        #     wr.writerow(["x", "y", "observed_b", "fitted_b", "weight"])
        #     for i in range(n):
        #         wr.writerow([
        #             f"{x[i]:.4f}", f"{y[i]:.4f}",
        #             f"{b_obs[i]:.6f}", f"{b_fit[i]:.6f}",
        #             f"{w_add[i]:.4f}",
        #         ])
        # self._logger.info("加性残差已保存: %s (%d 行)", add_path, n)


# ============================================================================
# 合成数据自测 / 验证
# 验证项:
#   1. GradientEstimator 可正常 import (不依赖外部 DLL)
#   2. 30 颗星 + 已知梯度, 传入模拟 PSF 结果 (不调用 DLL) 验证 calibrate() 流程
#   3. 返回 dict 含全部字段, 类型正确
#   4. 质量报告格式正确
#   5. 残差 CSV 可写入且可解析
#   6. 退化路径 (匹配星数 < 6) 返回退化结果
# ============================================================================
if __name__ == "__main__":
    import tempfile

    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")

    from wcs_transform import WCSTransform

    print("=" * 70)
    print("GradientEstimator 合成数据验证")
    print("=" * 70)

    # 临时日志/残差输出目录
    tmp_dir = tempfile.mkdtemp(prefix="gradient_estimator_test_")
    print(f"输出目录: {tmp_dir}")

    all_pass = True

    # ---- 构造 WCS (TAN 投影) ----
    crpix1, crpix2 = 100.0, 100.0   # 1-based
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.01                    # 度/像素
    wcs = WCSTransform(
        crpix1=crpix1, crpix2=crpix2,
        crval1=crval1, crval2=crval2,
        cd11=cd_val, cd12=0.0, cd21=0.0, cd22=cd_val,
    )

    img_w, img_h = 200, 200
    # ---- 30 颗星: 6 列 x 5 行网格, 像素坐标分布在 [25,175] ----
    px_vals = np.linspace(25, 175, 6)
    py_vals = np.linspace(25, 175, 5)
    px_grid, py_grid = np.meshgrid(px_vals, py_vals)
    px_flat = px_grid.ravel()
    py_flat = py_grid.ravel()
    n_stars = px_flat.size  # 30

    # 已知梯度 (归一化坐标):
    #   r_true = 0.10 + 0.05 * x_norm   (乘性, 仅依赖 x)
    #   b_true = 100  + 30  * y_norm    (加性, 仅依赖 y)
    x_norm = (2.0 * px_flat / img_w) - 1.0
    y_norm = (2.0 * py_flat / img_h) - 1.0
    r_true = 0.10 + 0.05 * x_norm
    b_true = 100.0 + 30.0 * y_norm

    # 合成流量 + 仪器流量: r = log10(F_instr / F_syn) = r_true
    # r = log10(M_true), M_true = 10^r_true, F_instr = F_syn * M_true
    f_syn_arr = np.full(n_stars, 50000.0)
    # 附加微小噪声使 r 有自然散布 (远小于离群阈值, 不触发清洗)
    rng = np.random.default_rng(42)
    noise = rng.normal(0.0, 0.002, n_stars)
    f_instr_arr = f_syn_arr * np.power(10.0, r_true + noise)

    # 构造 gaia_stars (ra/dec 由像素坐标反演, 保证 WCS 往返匹配)
    gaia_stars: list[dict] = []
    psf_results: list[dict] = []
    for i in range(n_stars):
        ra, dec = wcs.pixel_to_sky(float(px_flat[i]), float(py_flat[i]))
        gaia_stars.append({
            "ra": ra,
            "dec": dec,
            "mag_g": 12.0 + 0.1 * i,
            "f_syn": float(f_syn_arr[i]),
            "source_id": 1000 + i,
        })
        psf_results.append({
            "status": 0,
            "B": float(b_true[i]),
            "flux": float(f_instr_arr[i]),
            "cx": float(px_flat[i]),
            "cy": float(py_flat[i]),
        })

    # 原图: 200x200 uint16, 背景值 1000
    image = np.full((img_h, img_w), 1000, dtype=np.uint16)

    # ---- 验证 1: import 不依赖 DLL ----
    print("\n[验证 1] GradientEstimator 可正常 import (不依赖外部 DLL)")
    import_ok = "GradientEstimator" in globals() and "estimator" not in dir()
    # 模块顶层未导入 star_detector/dynamic_psf
    import sys as _sys
    top_clean = "star_detector" not in _sys.modules and "dynamic_psf" not in _sys.modules
    print(f"  顶层未加载 star_detector/dynamic_psf: {top_clean}")
    print(f"  [{'PASS' if import_ok and top_clean else 'FAIL'}] import 干净")
    all_pass = all_pass and import_ok and top_clean

    # ---- 验证 2: calibrate() 端到端流程 (30 颗星, 传入模拟 PSF 结果) ----
    print("\n[验证 2] calibrate() 端到端流程 (30 颗星, 已知梯度)")
    est = GradientEstimator(log_dir=tmp_dir, max_order=5)
    result = est.calibrate(image, gaia_stars, wcs, psf_results=psf_results)
    print(f"  n_matched={result['n_matched']}, n_excluded={result['n_excluded']}, "
          f"scale={result['scale_factor']:.6e}")
    flow_ok = (result["n_matched"] == 30 and result["n_excluded"] == 0
               and np.isfinite(result["scale_factor"]))
    print(f"  [{'PASS' if flow_ok else 'FAIL'}] 30 颗全匹配, 无离群排除")
    all_pass = all_pass and flow_ok

    # ---- 验证 3: 返回 dict 含全部字段且类型正确 ----
    print("\n[验证 3] 返回 dict 含全部字段且类型正确")
    required_keys = {
        "image_calibrated", "mult_surface", "add_surface", "scale_factor",
        "n_matched", "n_excluded", "quality_report",
    }
    keys_ok = required_keys.issubset(result.keys())
    img_cal = result["image_calibrated"]
    type_ok = (
        isinstance(img_cal, np.ndarray)
        and img_cal.dtype == np.float32
        and img_cal.shape == (img_h, img_w)
        and isinstance(result["mult_surface"], GradientSurface)
        and isinstance(result["add_surface"], GradientSurface)
        and isinstance(result["scale_factor"], float)
        and isinstance(result["n_matched"], int)
        and isinstance(result["n_excluded"], int)
        and isinstance(result["quality_report"], dict)
    )
    print(f"  必需键齐全: {keys_ok}")
    print(f"  image_calibrated: dtype={img_cal.dtype}, shape={img_cal.shape}")
    print(f"  mult/add_surface: {type(result['mult_surface']).__name__}")
    print(f"  [{'PASS' if keys_ok and type_ok else 'FAIL'}] 字段与类型")
    all_pass = all_pass and keys_ok and type_ok

    # ---- 验证 4: 乘性曲面恢复已知梯度 r_true ----
    print("\n[验证 4] 乘性曲面恢复已知梯度 r_true = 0.10 + 0.05*x_norm")

    def _coeff_for(surface: GradientSurface, j: int, k: int) -> float:
        """按单项式项 (x^j * y^k) 查找曲面系数, 避免依赖项序硬编码"""
        for idx, (jj, kk) in enumerate(
                GradientFitter._monomial_terms(surface.order)):
            if jj == j and kk == k:
                return float(surface.coeffs[idx])
        return float("nan")

    mult = result["mult_surface"]
    terms = GradientFitter._monomial_terms(mult.order)
    print(f"  拟合阶数={mult.order}, 项={terms}")
    # 单项式项 (j,k)=x^j*y^k; r_true = 0.10 + 0.05*x_norm
    #   (0,0)=常数 -> 0.10, (1,0)=x -> 0.05, (0,1)=y -> 0.0
    c_const = _coeff_for(mult, 0, 0)
    c_x = _coeff_for(mult, 1, 0)
    c_y = _coeff_for(mult, 0, 1)
    print(f"  常数项(0,0)={c_const:.4f} (期望 0.10), x项(1,0)={c_x:.4f} (期望 0.05), "
          f"y项(0,1)={c_y:.4f} (期望 ~0)")
    mult_ok = (abs(c_const - 0.10) < 0.02
               and abs(c_x - 0.05) < 0.02
               and abs(c_y) < 0.02)
    print(f"  残差中位数={mult.residual_median:.6e}, 残差std={mult.residual_std:.6e}")
    print(f"  [{'PASS' if mult_ok else 'FAIL'}] 乘性梯度系数恢复")
    all_pass = all_pass and mult_ok

    # ---- 验证 5: 加性曲面封存确认 (S_map=0) ----
    # [封存 2026-07-12] 原验证加性曲面恢复已知梯度，现已封存
    # 封存后 add_surface 为恒等曲面（order=1, coeffs 全0），S_map=0
    print("\n[验证 5] 加性曲面封存确认 (S_map=0)")
    add = result["add_surface"]
    add_frozen = (add.order == 1
                  and np.all(np.abs(add.coeffs) < 1e-12)
                  and add.n_used == 0
                  and np.isinf(add.loocv_error))
    print(f"  add_surface.order={add.order}, coeffs={add.coeffs}, "
          f"n_used={add.n_used}, loocv={add.loocv_error}")
    print(f"  [{'PASS' if add_frozen else 'FAIL'}] 加性梯度已封存 (恒等曲面)")
    all_pass = all_pass and add_frozen

    # ---- 验证 6: 质量报告格式正确 ----
    print("\n[验证 6] 质量报告格式正确")
    qr = result["quality_report"]
    qr_required = {
        "n_matched", "n_excluded", "n_used",
        "mult_order", "mult_loocv_error", "mult_residual_median",
        "mult_residual_std",
        "add_order", "add_loocv_error", "add_residual_median",
        "add_residual_std", "scale_factor",
    }
    qr_keys_ok = qr_required.issubset(qr.keys())
    qr_type_ok = (
        isinstance(qr["n_matched"], int)
        and isinstance(qr["n_excluded"], int)
        and isinstance(qr["n_used"], int)
        and isinstance(qr["mult_order"], int)
        and isinstance(qr["mult_loocv_error"], float)
        and isinstance(qr["mult_residual_median"], float)
        and isinstance(qr["mult_residual_std"], float)
        and isinstance(qr["add_order"], int)
        and isinstance(qr["add_loocv_error"], float)
        and isinstance(qr["scale_factor"], float)
    )
    print(f"  必需键齐全: {qr_keys_ok}")
    print(f"  n_matched={qr['n_matched']}, n_excluded={qr['n_excluded']}, "
          f"n_used={qr['n_used']}")
    print(f"  mult_order={qr['mult_order']}, mult_loocv={qr['mult_loocv_error']:.6e}")
    print(f"  add_order={qr['add_order']}, add_loocv={qr['add_loocv_error']:.6e}")
    print(f"  scale_factor={qr['scale_factor']:.6e}")
    print(f"  [{'PASS' if qr_keys_ok and qr_type_ok else 'FAIL'}] 质量报告格式")
    all_pass = all_pass and qr_keys_ok and qr_type_ok

    # ---- 验证 7: 残差 CSV (仅乘性, 加性已封存) ----
    # [封存 2026-07-12] add_residuals.csv 不再生成, 只验证乘性残差 CSV
    print("\n[验证 7] 残差 CSV (乘性, 加性已封存)")
    mult_csv = os.path.join(tmp_dir, "mult_residuals.csv")
    add_csv = os.path.join(tmp_dir, "add_residuals.csv")
    mult_exist = os.path.isfile(mult_csv)
    add_not_exist = not os.path.isfile(add_csv)  # 加性已封存, 文件不应存在

    def _read_csv_rows(path):
        with open(path, "r", encoding="utf-8") as f:
            rd = csv.reader(f)
            return list(rd)

    mult_rows = _read_csv_rows(mult_csv)
    mult_header_ok = mult_rows[0] == ["x", "y", "observed_r", "fitted_r", "weight"]
    mult_nrows_ok = len(mult_rows) == n_stars + 1
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
    print(f"  mult_residuals.csv: {len(mult_rows) - 1} 行, 表头正确={mult_header_ok}")
    print(f"  add_residuals.csv 已封存 (不存在={add_not_exist})")
    print(f"  数值可解析: {parse_ok}")
    csv_ok = (mult_exist and add_not_exist and mult_header_ok
              and mult_nrows_ok and parse_ok)
    print(f"  [{'PASS' if csv_ok else 'FAIL'}] 残差 CSV (乘性+加性封存)")
    all_pass = all_pass and csv_ok

    # ---- 验证 8: 退化路径 (匹配星数 < 6) ----
    print("\n[验证 8] 退化路径 (匹配星数 < 6)")
    # 构造 4 颗匹配星 (相同 r, 无离群), 期望退化结果
    few_gaia: list[dict] = []
    few_psf: list[dict] = []
    for i in range(4):
        px, py = 50.0 + i * 10.0, 50.0
        ra, dec = wcs.pixel_to_sky(px, py)
        f_syn_i = 50000.0
        f_instr_i = f_syn_i * (10.0 ** 0.1)  # r=0.1, 全相同 (F_instr = F_syn * M)
        few_gaia.append({
            "ra": ra, "dec": dec, "mag_g": 12.0, "f_syn": f_syn_i,
            "source_id": 2000 + i,
        })
        few_psf.append({
            "status": 0, "B": 100.0, "flux": f_instr_i,
            "cx": px, "cy": py,
        })
    est2 = GradientEstimator(log_dir=None)
    deg = est2.calibrate(image, few_gaia, wcs, psf_results=few_psf)
    deg_ok = (
        deg["n_matched"] == 4
        and deg["n_matched"] < 6
        and abs(deg["scale_factor"] - 1.0) < 1e-9
        and deg["mult_surface"].order == 1
        and np.allclose(deg["mult_surface"].coeffs, 0.0)
        and deg["image_calibrated"].dtype == np.float32
        and deg["quality_report"]["n_used"] == 0
    )
    print(f"  n_matched={deg['n_matched']}, scale={deg['scale_factor']:.1f}, "
          f"mult_order={deg['mult_surface'].order}, n_used={deg['quality_report']['n_used']}")
    print(f"  [{'PASS' if deg_ok else 'FAIL'}] 退化结果 (恒等曲面, scale=1.0)")
    all_pass = all_pass and deg_ok

    print("\n" + "=" * 70)
    print(f"验证结果: {'全部通过' if all_pass else '存在失败项'}")
    print("=" * 70)
